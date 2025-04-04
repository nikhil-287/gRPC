#include "data.grpc.pb.h"
#include "config_loader.h"
#include <grpcpp/grpcpp.h>
#include <iostream>
#include <memory>
#include <string>

using dataservice::DataRequest;
using dataservice::DataService;
using dataservice::Empty;
using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::Status;

RoutingConfig config;

class ForwardingServiceImpl final : public DataService::Service
{
public:
  Status SendData(ServerContext *context, const DataRequest *request, Empty *response) override
  {
    std::string data = request->payload();
    std::cout << "[Node " << config.node_name << "] Received: " << data << std::endl;

    // Forward to the next node(s) based on config
    if (config.routing_table.count(config.node_name))
    {
      for (const auto &neighbor : config.routing_table[config.node_name])
      {
        std::string address = config.address_map[neighbor];
        auto channel = grpc::CreateChannel(address, grpc::InsecureChannelCredentials());
        std::unique_ptr<DataService::Stub> stub = DataService::NewStub(channel);

        DataRequest forward_request;
        forward_request.set_payload(data);
        Empty forward_response;
        grpc::ClientContext ctx;

        Status status = stub->SendData(&ctx, forward_request, &forward_response);
        if (status.ok())
        {
          std::cout << "✅ Forwarded to " << neighbor << " (" << address << ")\n";
        }
        else
        {
          std::cerr << "❌ Failed to forward to " << neighbor << ": " << status.error_message() << std::endl;
        }
      }
    }

    return Status::OK;
  }
};

void RunServer()
{
  std::string server_address = "0.0.0.0:" + std::to_string(config.listen_port);
  ForwardingServiceImpl service;

  ServerBuilder builder;
  builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
  builder.RegisterService(&service);
  std::unique_ptr<Server> server(builder.BuildAndStart());
  std::cout << "[Node " << config.node_name << "] Listening on " << server_address << std::endl;
  server->Wait();
}

int main(int argc, char **argv)
{
  if (argc < 2)
  {
    std::cerr << "Usage: " << argv[0] << " <node_name>" << std::endl;
    return 1;
  }

  std::string node_name = argv[1];
  try
  {
    config = load_config("routing.json", node_name);
  }
  catch (const std::exception &ex)
  {
    std::cerr << "Failed to load config: " << ex.what() << std::endl;
    return 1;
  }

  RunServer();
  return 0;
}
