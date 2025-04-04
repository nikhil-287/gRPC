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

  RoutingConfig g_config; // Global config for forwarding

  void forward_to_neighbors(const std::string &payload)
  {
    for (const auto &neighbor : g_config.neighbors)
    {
      std::string address = g_config.address_map[neighbor];
      auto channel = grpc::CreateChannel(address, grpc::InsecureChannelCredentials());
      std::unique_ptr<DataService::Stub> stub = DataService::NewStub(channel);

      DataRequest request;
      request.set_payload(payload);
      Empty response;
      ClientContext context;

      Status status = stub->SendData(&context, request, &response);
      if (status.ok())
      {
        std::cout << "[Node " << g_config.node_name << "] Forwarded to " << neighbor << " at " << address << std::endl;
      }
      else
      {
        std::cerr << "[Node " << g_config.node_name << "] Failed to forward to " << neighbor << ": " << status.error_message() << std::endl;
      }
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

        // Forward from within the worker
        forward_to_neighbors(payload);
      }
    }
  }
}

void init_workers(int num_workers, const RoutingConfig &config)
{
  g_config = config; // Save for forwarding logic

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
      close(pipefd[1]); // child closes write
      worker_loop(pipefd[0], i);
      exit(0);
    }
    else
    {
      close(pipefd[0]); // parent closes read
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
