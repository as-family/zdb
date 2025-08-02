cmake_minimum_required(VERSION 4.0.0)

project(zdb
  VERSION 1.0.0
  DESCRIPTION "ZDB. Z for Zoza!"
  LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# --- Options ---
option(BUILD_TESTING "Build tests" ON)

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

set(SPDLOG_BUILD_EXAMPLE OFF CACHE BOOL "Build spdlog examples" FORCE)
set(SPDLOG_BUILD_TESTS OFF CACHE BOOL "Build spdlog tests" FORCE)
FetchContent_Declare(
  spdlog
  GIT_REPOSITORY https://github.com/gabime/spdlog.git
  GIT_TAG v1.15.3
  GIT_SHALLOW ON
)
FetchContent_MakeAvailable(spdlog)

add_executable(zdb)
target_compile_options(zdb PUBLIC
  -Wall
  -Wextra
  -Wpedantic
  -g
  -O0
  -Og

  -Wconversion
  -Wsign-conversion
  -Wcast-align
  -Wunused
  -Wshadow
  -Wold-style-cast
  -Woverloaded-virtual
  -Wnon-virtual-dtor
  -Wduplicated-cond
  -Wduplicated-branches
  -Wlogical-op
  -Wnull-dereference
)

target_sources(zdb PRIVATE
  src/main.cpp
  src/PeerNode.cpp
  src/proto/peer.proto
  src/InMemoryKVStore.cpp
  src/proto/kvStore.proto
  src/KVStoreServer.cpp
  src/KVStoreClient.cpp
  src/proto/error.proto
  src/Error.cpp
  src/ErrorConverter.cpp
  src/Repeater.cpp
  src/FullJitter.cpp
  src/ExponentialBackoff.cpp
  src/CircuitBreaker.cpp
  src/KVRPCService.cpp
  src/RetryPolicy.cpp
)

target_link_libraries(zdb PRIVATE
  protobuf::libprotobuf
  grpc++
  spdlog::spdlog
)

include("${grpc_BINARY_DIR}/third_party/protobuf/cmake/protobuf/protobuf-generate.cmake")
protobuf_generate(TARGET zdb)
protobuf_generate(
  TARGET zdb
  LANGUAGE grpc
  PLUGIN protoc-gen-grpc=$<TARGET_FILE:grpc_cpp_plugin>
  PLUGIN_OPTIONS generate_mock_code=true
  GENERATE_EXTENSIONS .grpc.pb.h .grpc.pb.cc
)
target_include_directories(zdb PRIVATE ${CMAKE_CURRENT_BINARY_DIR})

# --- Testing ---
if(BUILD_TESTING)
  FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG v1.17.0
    GIT_SHALLOW ON
  )

  # For Windows: Prevent overriding parent project's compiler/linker settings
  set(gtest_force_shared_crt ON CACHE BOOL "Force shared CRT" FORCE)
  set(BUILD_GMOCK ON CACHE BOOL "Build GMock" FORCE)
  set(INSTALL_GTEST OFF CACHE BOOL "Install GTest" FORCE)

  FetchContent_MakeAvailable(googletest)
  include(GoogleTest)

  # Only create test target if test files exist
  add_executable(tests)

  target_sources(tests PRIVATE
    src/proto/error.proto
    src/InMemoryKVStore.cpp
    src/ExponentialBackoff.cpp
    src/RetryPolicy.cpp
    src/FullJitter.cpp
    src/Error.cpp
    src/ErrorConverter.cpp
    src/Repeater.cpp
    src/CircuitBreaker.cpp
    src/KVRPCService.cpp
    src/proto/kvStore.proto
    src/KVStoreServer.cpp
    src/KVStoreServer.cpp
    src/KVStoreClient.cpp
    src/tst/InMemoryKVStoreTest.cpp
    src/tst/ExponentialBackoffTest.cpp
    src/tst/RetryPolicyTest.cpp
    src/tst/FullJitterTest.cpp
    src/tst/ErrorConverterTest.cpp
    src/tst/RepeaterTest.cpp
    src/tst/CircuitBreakerTest.cpp
    src/tst/KVRPCServiceTest.cpp
    src/tst/KVStoreServerTest.cpp
    src/tst/KVStoreClientTest.cpp
  )

  target_link_libraries(tests PRIVATE
    GTest::gtest_main
    GTest::gmock_main
    grpc++
    protobuf::libprotobuf
  )

  set_target_properties(tests PROPERTIES
    CXX_STANDARD 23
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
  )

  target_compile_options(tests PRIVATE
    -Wall
    -Wextra
    -Wpedantic
    -g
    -O0
    -Og

    -Wconversion
    -Wsign-conversion
    -Wcast-align
    -Wunused
    -Wshadow
    -Wold-style-cast
    -Woverloaded-virtual
    -Wnon-virtual-dtor
    -Wduplicated-cond
    -Wduplicated-branches
    -Wlogical-op
    -Wnull-dereference

    # -fsanitize=address
    # -fsanitize=undefined
    # -fsanitize=thread
    -fstack-protector-strong
    -D_FORTIFY_SOURCE=2
    )

    target_link_options(tests PRIVATE
      # -fsanitize=address
      # -fsanitize=undefined
      # -fsanitize=thread
  )

  protobuf_generate(TARGET tests)
  protobuf_generate(
    TARGET tests
    LANGUAGE grpc
    PLUGIN protoc-gen-grpc=$<TARGET_FILE:grpc_cpp_plugin>
    PLUGIN_OPTIONS generate_mock_code=true
    GENERATE_EXTENSIONS .grpc.pb.h .grpc.pb.cc
  )
  target_include_directories(tests PRIVATE ${CMAKE_CURRENT_BINARY_DIR})

  gtest_discover_tests(tests)
endif()

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
