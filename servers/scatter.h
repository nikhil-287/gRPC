#pragma once
#include <string>
#include "config_loader.h"

void init_workers(int num_workers, const RoutingConfig &config);
void scatter_payload(const std::string &payload);
void shutdown_workers();
