#include <iostream>
#include <memory>
#include <string>
#include <grpcpp/grpcpp.h>
#include "data.grpc.pb.h"
#include "scatter.h"
#include "config_loader.h"
#include "shared_data.h" // <-- Add this
#include <csignal>
#include <semaphore.h> // <-- Add this
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>

// These are needed to share the mutex and memory globally
SharedData *shared_data = nullptr;
sem_t *shared_mutex = nullptr;

using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::Status;

using dataservice::DataRequest;
using dataservice::DataService;
using dataservice::Empty;

RoutingConfig config;

void setup_shared_memory()
{
  // Only Node B creates it
  shm_unlink(SHM_NAME);
  sem_unlink(SEM_NAME);

  int fd = shm_open(SHM_NAME, O_CREAT | O_RDWR, 0666);
  if (fd == -1)
  {
    perror("shm_open");
    exit(1);
  }

  size_t page_size = sysconf(_SC_PAGESIZE);
  size_t data_size = sizeof(SharedData);
  size_t aligned_size = ((data_size + page_size - 1) / page_size) * page_size;

  std::cout << "[Node B] SharedData size: " << data_size << ", aligned to: " << aligned_size << std::endl;

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
  shared_data->num_neighbors = 0;

  shared_mutex = sem_open(SEM_NAME, O_CREAT, 0666, 1);
  if (shared_mutex == SEM_FAILED)
  {
    perror("sem_open");
    exit(1);
  }
}

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

  init_workers(3, config); // ✅ After memory init

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
    std::cout << "[Node B] 🛠 Config loaded successfully.\n";
    setup_shared_memory(); // ✅ ADD THIS
  }
  catch (const std::exception &ex)
  {
    std::cerr << "Failed to load config: " << ex.what() << std::endl;
    return 1;
  }

  signal(SIGINT, handle_sigint);
  RunServer();
  return 0;
}
