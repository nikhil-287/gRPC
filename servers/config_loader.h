#pragma once
#include <string>
#include <unordered_map>
#include <vector>

struct RoutingConfig
{
  std::string node_name;
  int listen_port;
  std::unordered_map<std::string, std::vector<std::string>> routing_table;
  std::unordered_map<std::string, std::string> address_map;
  std::vector<std::string> neighbors; // ✅ Add this
};

RoutingConfig load_config(const std::string &filepath, const std::string &node_name);
