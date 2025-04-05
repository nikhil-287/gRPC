#include "../servers/shared_data.h"
#include <iostream>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <cstring>
#include <semaphore.h>

int main()
{
  int fd = shm_open(SHM_NAME, O_RDWR, 0666);
  if (fd == -1)
  {
    perror("shm_open");
    return 1;
  }

  void *addr = mmap(NULL, sizeof(SharedData), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
  if (addr == MAP_FAILED)
  {
    perror("mmap");
    return 1;
  }

  auto *segment = static_cast<SharedData *>(addr);

  sem_t *mutex = sem_open(SEM_NAME, 0);
  if (mutex == SEM_FAILED)
  {
    perror("sem_open");
    return 1;
  }

  sem_wait(mutex);

  std::cout << "📊 Load Counts:\n";
  std::cout << "num_neighbors = " << segment->num_neighbors << std::endl;

  for (int i = 0; i < segment->num_neighbors; ++i)
  {
    std::cout << "  - " << segment->loads[i].name << ": " << segment->loads[i].load_count << " messages\n";
  }

  sem_post(mutex);

  munmap(addr, sizeof(SharedData));
  close(fd);
  return 0;
}
