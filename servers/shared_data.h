// === shared_data.h ===
#pragma once

#define SHM_NAME "/shared_load"
#define SEM_NAME "/shared_mutex"

#define MAX_NEIGHBORS 4
#define MAX_NAME_LEN 16

struct SharedLoad
{
  char name[MAX_NAME_LEN];
  int load_count;
};

struct SharedData
{
  SharedLoad loads[MAX_NEIGHBORS];
  int num_neighbors;
  char payload[1024]; // Optional, for payload tracking
  bool processed_by_c;
  bool processed_by_d;
  bool processed_by_e;
};