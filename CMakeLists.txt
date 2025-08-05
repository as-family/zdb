cmake_minimum_required(VERSION 3.31.0)

project(zdb
  VERSION 0.0.1
  DESCRIPTION "ZDB. Z for Zoza!"
  LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Store build directory in a cache variable for easy querying
set(ZDB_BUILD_DIR "${CMAKE_BINARY_DIR}" CACHE PATH "ZDB build directory")

set(BUILD_TESTING ON)

include(FetchContent)
include(CTest)

set(FETCHCONTENT_QUIET OFF)

set(gRPC_BUILD_TESTS OFF CACHE BOOL "Build gRPC tests" FORCE)
set(RE2_BUILD_TESTING OFF CACHE BOOL "Build re tests" FORCE)
set(gRPC_INSTALL ON CACHE BOOL "Install gRPC" FORCE)
set(gRPC_BUILD_GRPC_CPP_PLUGIN ON CACHE BOOL "Build grpc_cpp_plugin" FORCE)
FetchContent_Declare(
  grpc
  GIT_REPOSITORY https://github.com/grpc/grpc.git
  GIT_TAG v1.73.1
  GIT_SHALLOW ON
)
FetchContent_MakeAvailable(grpc)
include("${grpc_BINARY_DIR}/third_party/protobuf/cmake/protobuf/protobuf-generate.cmake")

set(SPDLOG_BUILD_EXAMPLE OFF CACHE BOOL "Build spdlog examples" FORCE)
set(SPDLOG_BUILD_TESTS OFF CACHE BOOL "Build spdlog tests" FORCE)
FetchContent_Declare(
  spdlog
  GIT_REPOSITORY https://github.com/gabime/spdlog.git
  GIT_TAG v1.15.3
  GIT_SHALLOW ON
)
FetchContent_MakeAvailable(spdlog)

add_library(protobuf_generated STATIC)
target_link_libraries(protobuf_generated PRIVATE
  protobuf::libprotobuf
  grpc++
)
target_sources(protobuf_generated PRIVATE
  ${CMAKE_SOURCE_DIR}/proto/error.proto
  ${CMAKE_SOURCE_DIR}/proto/kvStore.proto
  ${CMAKE_SOURCE_DIR}/proto/peer.proto
)
protobuf_generate(
  TARGET protobuf_generated
)
protobuf_generate(
  TARGET protobuf_generated
  LANGUAGE grpc
  PLUGIN protoc-gen-grpc=$<TARGET_FILE:grpc_cpp_plugin>
  PLUGIN_OPTIONS generate_mock_code=true
  GENERATE_EXTENSIONS .grpc.pb.h .grpc.pb.cc
)
target_include_directories(protobuf_generated PRIVATE
  ${CMAKE_BINARY_DIR}
)

add_subdirectory(src)
add_subdirectory(tst)

# --- Installation ---
install(TARGETS zdb
  RUNTIME DESTINATION bin
  LIBRARY DESTINATION lib
  ARCHIVE DESTINATION lib
)

# --- Summary ---
message(STATUS "")
message(STATUS "Configuration Summary:")
message(STATUS "  Project: ${PROJECT_NAME} v${PROJECT_VERSION}")
message(STATUS "  Build Type: ${CMAKE_BUILD_TYPE}")
message(STATUS "  C++ Standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "  Build Testing: ${BUILD_TESTING}")
message(STATUS "")
