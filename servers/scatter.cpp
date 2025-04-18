#include "scatter.h"
#include "config_loader.h"
#include "data.grpc.pb.h"
#include "shared_data.h"

#include <grpcpp/grpcpp.h>
#include <iostream>
#include <vector>
#include <semaphore.h>
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

extern SharedData *shared_data;
extern sem_t *shared_mutex;

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

  void forward_to_neighbors(const std::string &payload)
  {
    for (const auto &neighbor : g_config.neighbors)
    {
      std::string address = g_config.address_map[neighbor];
      auto channel = grpc::CreateChannel(address, grpc::InsecureChannelCredentials());

      if (!channel->WaitForConnected(gpr_time_add(gpr_now(GPR_CLOCK_REALTIME), gpr_time_from_seconds(2, GPR_TIMESPAN))))
      {
        std::cerr << "[Scatter] ❌ Timeout while connecting to " << neighbor << " at " << address << std::endl;
        continue;
      }

      std::unique_ptr<DataService::Stub> stub = DataService::NewStub(channel);

      DataRequest request;
      request.set_payload(payload);
      Empty response;
      ClientContext context;

      std::cout << "[Scatter] 🔁 Sending to " << neighbor << " at " << address << std::endl;

      Status status = stub->SendData(&context, request, &response);
      if (status.ok())
      {
        std::cout << "[Scatter] ✅ Successfully sent to " << neighbor << std::endl;

        // 🔐 Update shared memory load tracking
        if (shared_mutex && shared_data)
        {
          sem_wait(shared_mutex);

          bool found = false;
          for (int i = 0; i < shared_data->num_neighbors; ++i)
          {
            if (strcmp(shared_data->loads[i].name, neighbor.c_str()) == 0)
            {
              shared_data->loads[i].load_count++;
              found = true;
              break;
            }
          }

          if (!found && shared_data->num_neighbors < MAX_NEIGHBORS)
          {
            strncpy(shared_data->loads[shared_data->num_neighbors].name, neighbor.c_str(), MAX_NAME_LEN - 1);
            shared_data->loads[shared_data->num_neighbors].name[MAX_NAME_LEN - 1] = '\0';
            shared_data->loads[shared_data->num_neighbors].load_count = 1;
            shared_data->num_neighbors++;
          }

          sem_post(shared_mutex);
        }
      }
      else
      {
        std::cerr << "[Scatter] ❌ Failed to send to " << neighbor << ": " << status.error_message() << std::endl;
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

        forward_to_neighbors(payload);
      }
    }
  }
}

void init_workers(int num_workers, const RoutingConfig &config)
{
  g_config = config;

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
