#include <iostream>
#include <memory>
#include <string>
#include <grpcpp/grpcpp.h>
#include "data.grpc.pb.h"
#include "scatter.h"
#include "config_loader.h"
#include <csignal>

using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::Status;

using dataservice::DataRequest;
using dataservice::DataService;
using dataservice::Empty;

RoutingConfig config;

class DataServiceImpl final : public DataService::Service
{
public:
  Status SendData(ServerContext *context, const DataRequest *request, Empty *response) override
  {
    std::cout << "[Node B] Received payload: " << request->payload() << std::endl;
    scatter_payload(request->payload());
    return Status::OK;
  }
};

void RunServer()
{
  std::string address("0.0.0.0:50052");
  DataServiceImpl service;

  init_workers(3, config); // ✅ Pass the config here

  ServerBuilder builder;
  builder.AddListeningPort(address, grpc::InsecureServerCredentials());
  builder.RegisterService(&service);

  std::unique_ptr<Server> server(builder.BuildAndStart());
  std::cout << "[Node B] Server listening on " << address << std::endl;
  server->Wait();
}

void handle_sigint(int)
{
  shutdown_workers();
  std::cout << "[Node B] Exiting.\n";
  exit(0);
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
