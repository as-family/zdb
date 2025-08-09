cmake_minimum_required(VERSION 3.31.0)

project(zdb
  VERSION 0.0.1
  DESCRIPTION "ZDB. Z for Zoza!"
  LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Disable C++ modules until clang-tidy supports them
set(CMAKE_CXX_SCAN_FOR_MODULES OFF)

# Store build directory in a cache variable for easy querying
set(ZDB_BUILD_DIR "${CMAKE_BINARY_DIR}" CACHE PATH "ZDB build directory")

set(BUILD_TESTING ON)

# Find packages using vcpkg
find_package(Protobuf CONFIG REQUIRED)
find_package(gRPC CONFIG REQUIRED)
find_package(spdlog CONFIG REQUIRED)

add_library(protobuf_generated STATIC)
target_sources(protobuf_generated PRIVATE
  ${CMAKE_SOURCE_DIR}/proto/error.proto
  ${CMAKE_SOURCE_DIR}/proto/kvStore.proto
  ${CMAKE_SOURCE_DIR}/proto/peer.proto
)

# Generate protobuf C++ files
protobuf_generate(
  TARGET protobuf_generated
)

# Generate gRPC C++ files
protobuf_generate(
  TARGET protobuf_generated
  LANGUAGE grpc
  PLUGIN protoc-gen-grpc=$<TARGET_FILE:gRPC::grpc_cpp_plugin>
  PLUGIN_OPTIONS generate_mock_code=true
  GENERATE_EXTENSIONS .grpc.pb.h .grpc.pb.cc
)

target_link_libraries(protobuf_generated PRIVATE
  protobuf::libprotobuf
  gRPC::grpc++
)

target_include_directories(protobuf_generated PRIVATE
  ${CMAKE_BINARY_DIR}
)

add_subdirectory(src)
add_subdirectory(tst)

# Run post-test static analysis: cppcheck and run-clang-tidy.sh
# Usage: cmake --build <build_dir> --target post-test-analysis
add_custom_target(post-test-analysis
  DEPENDS tests
  COMMAND ${CMAKE_COMMAND} -E echo "Running tests (ctest), then post-test static analysis..."
  COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure
  # cppcheck via shared script (skips gracefully if cppcheck not installed)
  COMMAND bash -lc "if [ -f '${CMAKE_SOURCE_DIR}/run-cppcheck.sh' ]; then \
    chmod +x '${CMAKE_SOURCE_DIR}/run-cppcheck.sh'; \
    '${CMAKE_SOURCE_DIR}/run-cppcheck.sh' '${CMAKE_BINARY_DIR}' '${CMAKE_SOURCE_DIR}/cppcheck-report.xml'; \
  else \
    echo 'run-cppcheck.sh not found at ${CMAKE_SOURCE_DIR}, skipping cppcheck.'; \
  fi"
  # run-clang-tidy.sh (skips gracefully if missing)
  COMMAND bash -lc "if [ -f '${CMAKE_SOURCE_DIR}/run-clang-tidy.sh' ]; then \
    echo 'Running run-clang-tidy.sh...'; \
  chmod +x '${CMAKE_SOURCE_DIR}/run-clang-tidy.sh'; \
  bash '${CMAKE_SOURCE_DIR}/run-clang-tidy.sh' '${CMAKE_BINARY_DIR}' both; \
  else \
    echo 'run-clang-tidy.sh not found at ${CMAKE_SOURCE_DIR}, skipping.'; \
  fi"
  COMMENT "Run cppcheck and run-clang-tidy.sh after tests"
  VERBATIM
)

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
message(STATUS "  Target 'post-test-analysis': runs tests, then cppcheck and run-clang-tidy.sh")
message(STATUS "")
