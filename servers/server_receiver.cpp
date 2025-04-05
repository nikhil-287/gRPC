#include "data.grpc.pb.h"
#include "config_loader.h"
#include "shared_data.h"
#include <semaphore.h>

#include <grpcpp/grpcpp.h>
#include <iostream>
#include <memory>
#include <string>
#include <cstring>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>

using dataservice::DataRequest;
using dataservice::DataService;
using dataservice::Empty;
using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::Status;

RoutingConfig config;
SharedData *shared_data = nullptr;
sem_t *shared_mutex = nullptr;

void setup_shared_memory()
{
  shm_unlink(SHM_NAME); // Remove existing shared memory
  sem_unlink(SEM_NAME); // Remove existing semaphore

  int fd = shm_open(SHM_NAME, O_CREAT | O_RDWR, 0666);
  if (fd == -1)
  {
    perror("shm_open");
    exit(1);
  }

  size_t page_size = sysconf(_SC_PAGESIZE);
  size_t data_size = sizeof(SharedData);
  size_t aligned_size = ((data_size + page_size - 1) / page_size) * page_size;

  std::cout << "SharedData size: " << data_size << ", aligned to: " << aligned_size << std::endl;

  if (ftruncate(fd, aligned_size) == -1)
  {
    perror("ftruncate");
    exit(1);
  }

  void *ptr = mmap(nullptr, aligned_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
  if (ptr == MAP_FAILED)
  {
    perror("mmap");
    exit(1);
  }

  shared_data = reinterpret_cast<SharedData *>(ptr);
  memset(shared_data, 0, sizeof(SharedData)); // ✅ Clear old memory

  // ✅ Initialize from config
  shared_data->num_neighbors = static_cast<int>(config.neighbors.size());
  for (int i = 0; i < shared_data->num_neighbors; ++i)
  {
    strncpy(shared_data->loads[i].name, config.neighbors[i].c_str(), MAX_NAME_LEN - 1);
    shared_data->loads[i].name[MAX_NAME_LEN - 1] = '\0';
    shared_data->loads[i].load_count = 0;
  }

  shared_mutex = sem_open(SEM_NAME, O_CREAT, 0666, 1);
  if (shared_mutex == SEM_FAILED)
  {
    perror("sem_open");
    exit(1);
  }
}

class ReceiverServiceImpl final : public DataService::Service
{
public:
  Status SendData(ServerContext *context, const DataRequest *request, Empty *response) override
  {
    std::string payload = request->payload();
    std::cout << "[Node " << config.node_name << "] ✅ Received payload: " << payload << std::endl;

    // Store in shared memory
    if (shared_data)
    {
      strncpy(shared_data->payload, payload.c_str(), sizeof(shared_data->payload) - 1);
      shared_data->payload[sizeof(shared_data->payload) - 1] = '\0';

      if (config.node_name == "C")
      {
        shared_data->processed_by_c = true;
      }
      else if (config.node_name == "D")
      {
        shared_data->processed_by_d = true;
      }
      else if (config.node_name == "E")
      {
        shared_data->processed_by_e = true;
      }
    }

    // Forward to neighbors if any
    auto it = config.routing_table.find(config.node_name);
    if (it != config.routing_table.end())
    {
      for (const auto &neighbor : it->second)
      {
        std::string neighbor_address = config.address_map[neighbor];
        auto channel = grpc::CreateChannel(neighbor_address, grpc::InsecureChannelCredentials());
        std::unique_ptr<DataService::Stub> stub = DataService::NewStub(channel);

        DataRequest forward_request;
        forward_request.set_payload(payload);
        Empty forward_response;
        grpc::ClientContext ctx;

        grpc::Status status = stub->SendData(&ctx, forward_request, &forward_response);
        if (status.ok())
        {
          std::cout << "  → Forwarded to " << neighbor << " (" << neighbor_address << ")" << std::endl;
        }
        else
        {
          std::cerr << "  ✖ Failed to forward to " << neighbor << ": " << status.error_message() << std::endl;
        }
      }
    }

    return Status::OK;
  }
};

void RunServer()
{
  std::string server_address = "0.0.0.0:" + std::to_string(config.listen_port);
  ReceiverServiceImpl service;

  ServerBuilder builder;
  builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
  builder.RegisterService(&service);

  std::unique_ptr<Server> server(builder.BuildAndStart());
  std::cout << "[Node " << config.node_name << "] 🚀 Listening on " << server_address << std::endl;
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
    std::cout << "[Node " << node_name << "] 🛠 Config loaded successfully.\n";
    setup_shared_memory(); // ✅ Shared memory init
  }
  catch (const std::exception &ex)
  {
    std::cerr << "❌ Failed to load config: " << ex.what() << std::endl;
    return 1;
  }

  RunServer();
  return 0;
}
