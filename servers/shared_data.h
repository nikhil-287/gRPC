#pragma once

#define SHM_NAME "/shared_load"
#define SEM_NAME "/shared_mutex"

#define MAX_NEIGHBORS 4
#define MAX_NAME_LEN 16
#define MAX_PAYLOADS 2048
#define MAX_PAYLOAD_LEN 1024

struct SharedLoad
{
  char name[MAX_NAME_LEN];
  int load_count;
};

struct SharedData
{
  SharedLoad loads[MAX_NEIGHBORS];
  int num_neighbors;

  // Per-node seen payload tracking
  char seen_payloads_c[MAX_PAYLOADS][MAX_PAYLOAD_LEN];
  int count_c;

  char seen_payloads_d[MAX_PAYLOADS][MAX_PAYLOAD_LEN];
  int count_d;

  char seen_payloads_e[MAX_PAYLOADS][MAX_PAYLOAD_LEN];
  int count_e;

  char seen_payloads_f[MAX_PAYLOADS][MAX_PAYLOAD_LEN];
  int count_f;

  char payload[MAX_PAYLOAD_LEN]; // Optional: most recent payload
};