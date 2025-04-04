#include <iostream>
#include <memory>
#include <string>

#include <grpcpp/grpcpp.h>
#include "data.grpc.pb.h"

using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::Status;

using dataservice::DataRequest;
using dataservice::DataService;
using dataservice::Empty;

class DataServiceImpl final : public DataService::Service
{
public:
  Status SendData(ServerContext *context, const DataRequest *request, Empty *response) override
  {
    std::string payload = request->payload();
    std::cout << "[Node A] Received: " << payload << std::endl;

    // TODO: Forward this payload to Node B via gRPC
    return Status::OK;
  }
};

void RunServer()
{
  std::string server_address("0.0.0.0:50051");
  DataServiceImpl service;

  ServerBuilder builder;
  builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
  builder.RegisterService(&service);

  std::unique_ptr<Server> server(builder.BuildAndStart());
  std::cout << "[Node A] Server listening on " << server_address << std::endl;
  server->Wait();
}

int main()
{
  RunServer();
  return 0;
}
