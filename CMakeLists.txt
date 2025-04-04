cmake_minimum_required(VERSION 3.10)
project(mini2_system)

set(CMAKE_CXX_STANDARD 17)

include_directories(/opt/homebrew/include)
link_directories(/opt/homebrew/lib)

# === Executables ===
add_executable(server_a_forwarding
  servers/server_a_forwarding.cpp
  servers/config_loader.cpp
  servers/data.pb.cc
  servers/data.grpc.pb.cc
)

add_executable(server_b
  servers/server_b.cpp
  servers/scatter.cpp
  servers/data.pb.cc
  servers/config_loader.cpp
  servers/data.grpc.pb.cc
)

add_executable(server_c
  servers/server_receiver.cpp
  servers/config_loader.cpp
  servers/data.pb.cc
  servers/data.grpc.pb.cc
)

add_executable(server_d
  servers/server_receiver.cpp
  servers/config_loader.cpp
  servers/data.pb.cc
  servers/data.grpc.pb.cc
)

add_executable(server_e
  servers/server_receiver.cpp
  servers/config_loader.cpp
  servers/data.pb.cc
  servers/data.grpc.pb.cc
)

# === Common include path ===
target_include_directories(server_a_forwarding PRIVATE servers/)

# === Dependencies ===
set(GRPC_DEPS
  grpc++
  grpc
  gpr
  protobuf
  absl_strings
  absl_time
  absl_base
  absl_synchronization
  absl_symbolize
  absl_stacktrace
  absl_malloc_internal
  absl_throw_delegate
  absl_log_internal_check_op
  absl_log_internal_message
  absl_log_internal_globals
  absl_log_internal_nullguard
  absl_log_severity
  absl_cord
  absl_cord_internal
  absl_cordz_info
  absl_cordz_handle
  absl_cordz_functions
  absl_cordz_sample_token
  z
)

# === Link all servers ===
foreach(target IN ITEMS server_a_forwarding server_b server_c server_d server_e)
  target_link_libraries(${target} ${GRPC_DEPS})
endforeach()
