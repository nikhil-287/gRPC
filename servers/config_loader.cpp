#include "config_loader.h"
#include <fstream>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

RoutingConfig load_config(const std::string &filepath, const std::string &node_name)
{
  std::ifstream in(filepath);
  if (!in.is_open())
  {
    throw std::runtime_error("Failed to open config file");
  }

  json j;
  in >> j;

  if (!j["nodes"].contains(node_name))
  {
    throw std::runtime_error("Node config not found: " + node_name);
  }

  RoutingConfig config;
  config.node_name = node_name;
  config.listen_port = j["nodes"][node_name]["listen_port"];

  for (auto &[key, val] : j["routing_table"].items())
  {
    for (auto &dst : val)
    {
      config.routing_table[key].push_back(dst);
    }
  }

  for (auto &[key, val] : j["address_map"].items())
  {
    config.address_map[key] = val;
  }

  // ✅ Extract neighbors specific to this node
  if (j["routing_table"].contains(node_name))
  {
    for (auto &dst : j["routing_table"][node_name])
    {
      config.neighbors.push_back(dst.get<std::string>());
    }
  }

  return config;
}
