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
#include <fstream>
#include <climits>
#include <chrono>
#include <csignal>

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

enum class LoadStrategy
{
  RoundRobin,
  LeastLoaded
};
LoadStrategy strategy = LoadStrategy::LeastLoaded;
int rr_index = 0;

// Benchmarking
std::chrono::steady_clock::time_point server_start_time;
int processed_count = 0;
int duplicate_count = 0;
int forwarded_count = 0;

void write_benchmark_and_exit(int signum)
{
  auto end_time = std::chrono::steady_clock::now();
  auto duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - server_start_time).count();

  std::ofstream bench("benchmark_" + config.node_name + ".txt", std::ios::out);
  bench << "Messages Processed: " << processed_count << "\n";
  bench << "Duplicates Skipped: " << duplicate_count << "\n";
  bench << "Messages Forwarded: " << forwarded_count << "\n";
  bench << "Total Time (ms): " << duration_ms << "\n";
  if (duration_ms > 0)
  {
    bench << "Throughput (msg/sec): " << (processed_count * 1000 / duration_ms) << "\n";
  }
  bench.flush();
  bench.close();
  std::cout << "\n📈 Benchmark written to benchmark_" << config.node_name << ".txt. Exiting...\n";
  exit(signum);
}

void setup_shared_memory()
{
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
  memset(shared_data, 0, sizeof(SharedData));

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

bool is_duplicate(const std::string &node, const std::string &payload)
{
  char(*list)[MAX_PAYLOAD_LEN] = nullptr;
  int count = 0;

  if (node == "C")
  {
    list = shared_data->seen_payloads_c;
    count = shared_data->count_c;
  }
  else if (node == "D")
  {
    list = shared_data->seen_payloads_d;
    count = shared_data->count_d;
  }
  else if (node == "E")
  {
    list = shared_data->seen_payloads_e;
    count = shared_data->count_e;
  }
  else if (node == "F")
  {
    list = shared_data->seen_payloads_f;
    count = shared_data->count_f;
  }

  for (int i = 0; i < count; ++i)
  {
    if (strncmp(list[i], payload.c_str(), MAX_PAYLOAD_LEN) == 0)
    {
      return true;
    }
  }

  return false;
}

void mark_processed(const std::string &node, const std::string &payload)
{
  char(*list)[MAX_PAYLOAD_LEN] = nullptr;
  int *count = nullptr;

  if (node == "C")
  {
    list = shared_data->seen_payloads_c;
    count = &shared_data->count_c;
  }
  else if (node == "D")
  {
    list = shared_data->seen_payloads_d;
    count = &shared_data->count_d;
  }
  else if (node == "E")
  {
    list = shared_data->seen_payloads_e;
    count = &shared_data->count_e;
  }
  else if (node == "F")
  {
    list = shared_data->seen_payloads_f;
    count = &shared_data->count_f;
  }

  if (count && *count < MAX_PAYLOADS)
  {
    strncpy(list[*count], payload.c_str(), MAX_PAYLOAD_LEN - 1);
    list[*count][MAX_PAYLOAD_LEN - 1] = '\0';
    (*count)++;
  }
}

class ReceiverServiceImpl final : public DataService::Service
{
public:
  Status SendData(ServerContext *context, const DataRequest *request, Empty *response) override
  {
    std::string payload = request->payload();
    std::cout << "[Node " << config.node_name << "] ✅ Received payload: " << payload << std::endl;

    sem_wait(shared_mutex);
    bool is_dup = is_duplicate(config.node_name, payload);
    sem_post(shared_mutex);

    if (is_dup)
    {
      duplicate_count++;
      std::cout << "[Node " << config.node_name << "] ⚠️ Duplicate payload. Skipping.\n";
      std::ofstream dup("duplicates.txt", std::ios::app);
      dup << "[Node " << config.node_name << "] Duplicate: " << payload << "\n";
      return Status::OK;
    }

    sem_wait(shared_mutex);
    mark_processed(config.node_name, payload);
    sem_post(shared_mutex);
    processed_count++;

    std::ofstream out("node_" + config.node_name + "_data.txt", std::ios::app);
    out << payload << "\n";

    if (config.node_name != "E" && config.node_name != "F")
    {
      std::string selected_neighbor;

      sem_wait(shared_mutex);
      if (shared_data->num_neighbors > 0)
      {
        if (strategy == LoadStrategy::RoundRobin)
        {
          selected_neighbor = shared_data->loads[rr_index % shared_data->num_neighbors].name;
          rr_index = (rr_index + 1) % shared_data->num_neighbors;
          std::cout << "[Node " << config.node_name << "] 🔄 Round Robin → " << selected_neighbor << std::endl;
        }
        else
        {
          int min_load = INT_MAX;
          for (int i = 0; i < shared_data->num_neighbors; ++i)
          {
            if (shared_data->loads[i].load_count < min_load)
            {
              min_load = shared_data->loads[i].load_count;
              selected_neighbor = shared_data->loads[i].name;
            }
          }
          std::cout << "[Node " << config.node_name << "] ⚖️ Least Loaded → " << selected_neighbor << std::endl;
        }
      }
      sem_post(shared_mutex);

      if (!selected_neighbor.empty())
      {
        std::string neighbor_address = config.address_map[selected_neighbor];
        auto channel = grpc::CreateChannel(neighbor_address, grpc::InsecureChannelCredentials());
        std::unique_ptr<DataService::Stub> stub = DataService::NewStub(channel);

        DataRequest forward_request;
        forward_request.set_payload(payload);
        Empty forward_response;
        grpc::ClientContext ctx;

        grpc::Status status = stub->SendData(&ctx, forward_request, &forward_response);
        if (status.ok())
        {
          std::cout << "  → Forwarded to " << selected_neighbor << " (" << neighbor_address << ")" << std::endl;
          forwarded_count++;

          sem_wait(shared_mutex);
          for (int i = 0; i < shared_data->num_neighbors; ++i)
          {
            if (selected_neighbor == shared_data->loads[i].name)
            {
              shared_data->loads[i].load_count++;
              break;
            }
          }
          sem_post(shared_mutex);
        }
        else
        {
          std::cerr << "  ✖ Failed to forward to " << selected_neighbor << ": " << status.error_message() << std::endl;
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

  std::unique_ptr<Server> server = builder.BuildAndStart();
  std::cout << "[Node " << config.node_name << "] 🚀 Listening on " << server_address << std::endl;
  server->Wait();
}

int main(int argc, char **argv)
{
  if (argc < 2)
  {
    std::cerr << "Usage: " << argv[0] << " <node_name> [roundrobin|leastloaded]" << std::endl;
    return 1;
  }

  std::string node_name = argv[1];
  std::string strategy_arg = (argc >= 3) ? argv[2] : "leastloaded";

  if (strategy_arg == "roundrobin")
    strategy = LoadStrategy::RoundRobin;
  else if (strategy_arg == "leastloaded")
    strategy = LoadStrategy::LeastLoaded;
  else
  {
    std::cerr << "❌ Invalid strategy: " << strategy_arg << std::endl;
    return 1;
  }

  try
  {
    config = load_config("routing.json", node_name);
    std::cout << "[Node " << node_name << "] 🛠 Config loaded successfully.\n";
    setup_shared_memory();
    server_start_time = std::chrono::steady_clock::now();
    signal(SIGINT, write_benchmark_and_exit);
  }
  catch (const std::exception &ex)
  {
    std::cerr << "❌ Failed to load config: " << ex.what() << std::endl;
    return 1;
  }

  RunServer();
  return 0;
}