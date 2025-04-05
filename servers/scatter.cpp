#include "scatter.h"
#include "config_loader.h"
#include "data.grpc.pb.h"

#include <grpcpp/grpcpp.h>
#include <iostream>
#include <vector>
#include <unistd.h>
#include <sys/wait.h>
#include <csignal>
#include <cstring>
#include <map>
#include <mutex>

using dataservice::DataRequest;
using dataservice::DataService;
using dataservice::Empty;
using grpc::Channel;
using grpc::ClientContext;
using grpc::Status;

namespace
{
  struct Worker
  {
    int write_fd;
    pid_t pid;
  };

  std::vector<Worker> workers;
  int current_worker = 0;
  RoutingConfig g_config;

  std::map<std::string, int> load_map;

  // No shared stubs or channels across forks

  std::string get_least_loaded_neighbor()
  {
    std::string min_node;
    int min_load = INT32_MAX;
    for (const auto &neighbor : g_config.neighbors)
    {
      if (load_map[neighbor] < min_load)
      {
        min_load = load_map[neighbor];
        min_node = neighbor;
      }
    }
    return min_node;
  }

  void forward_to_least_loaded(const std::string &payload)
  {
    std::string target_node = get_least_loaded_neighbor();
    std::string address = g_config.address_map[target_node];

    auto channel = grpc::CreateChannel(address, grpc::InsecureChannelCredentials());
    std::unique_ptr<DataService::Stub> stub = DataService::NewStub(channel);

    DataRequest request;
    request.set_payload(payload);
    Empty response;
    ClientContext context;

    std::cout << "[Scatter] 🔁 Sending to " << target_node << " at " << address << std::endl;

    Status status = stub->SendData(&context, request, &response);
    if (status.ok())
    {
      std::cout << "[Scatter] ✅ Forwarded to " << target_node << std::endl;
      load_map[target_node]++;
    }
    else
    {
      std::cerr << "[Scatter] ❌ Failed to forward to " << target_node << ": " << status.error_message() << std::endl;
    }
  }

  void worker_loop(int read_fd, int id)
  {
    char buffer[1024];
    while (true)
    {
      ssize_t bytes = read(read_fd, buffer, sizeof(buffer) - 1);
      if (bytes > 0)
      {
        buffer[bytes] = '\0';
        std::string payload(buffer);
        std::cout << "[Worker " << id << "] received: " << payload << std::endl;
        forward_to_least_loaded(payload);
      }
    }
  }
}

void init_workers(int num_workers, const RoutingConfig &config)
{
  g_config = config;
  load_map.clear();
  for (const auto &neighbor : config.neighbors)
  {
    load_map[neighbor] = 0;
  }

  std::cout << "[Scatter] Initialized stubs for neighbors: ";
  for (const auto &n : config.neighbors)
    std::cout << n << " ";
  std::cout << std::endl;

  for (int i = 0; i < num_workers; ++i)
  {
    int pipefd[2];
    if (pipe(pipefd) == -1)
    {
      perror("pipe");
      exit(1);
    }

    pid_t pid = fork();
    if (pid == -1)
    {
      perror("fork");
      exit(1);
    }
    else if (pid == 0)
    {
      close(pipefd[1]);
      worker_loop(pipefd[0], i);
      exit(0);
    }
    else
    {
      close(pipefd[0]);
      workers.push_back({pipefd[1], pid});
    }
  }

  std::cout << "Initialized " << workers.size() << " workers.\n";
}

void scatter_payload(const std::string &payload)
{
  if (workers.empty())
    return;

  int target = current_worker % workers.size();
  current_worker++;

  write(workers[target].write_fd, payload.c_str(), payload.size());
}

void shutdown_workers()
{
  std::cout << "\n[Node B] Shutting down workers..." << std::endl;

  for (const auto &worker : workers)
  {
    close(worker.write_fd);
    kill(worker.pid, SIGTERM);
  }

  for (const auto &worker : workers)
  {
    waitpid(worker.pid, nullptr, 0);
    std::cout << "  ✔ Worker " << worker.pid << " exited cleanly.\n";
  }

  workers.clear();
}
