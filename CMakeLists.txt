cmake_minimum_required(VERSION 3.31.0)

project(zdb
  VERSION 0.0.1
  DESCRIPTION "ZDB. Z for Zoza!"
  LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD 23)

# Disable C++ modules until clang-tidy supports them
set(CMAKE_CXX_SCAN_FOR_MODULES OFF)

# Store build directory in a cache variable for easy querying
set(ZDB_BUILD_DIR "${CMAKE_BINARY_DIR}" CACHE PATH "ZDB build directory")

set(BUILD_TESTING ON)

# Find packages using vcpkg
find_package(Protobuf CONFIG REQUIRED)
find_package(gRPC CONFIG REQUIRED)
find_package(spdlog CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)

add_library(protobuf_generated STATIC)

# Generate protobuf C++ files
protobuf_generate(
  TARGET protobuf_generated
  PROTOS
    ${CMAKE_SOURCE_DIR}/proto/types.proto
    ${CMAKE_SOURCE_DIR}/proto/error.proto
    ${CMAKE_SOURCE_DIR}/proto/kvStore.proto
    ${CMAKE_SOURCE_DIR}/proto/raft.proto
)

# Generate gRPC C++ files
protobuf_generate(
  TARGET protobuf_generated
  LANGUAGE grpc
  PLUGIN protoc-gen-grpc=$<TARGET_FILE:gRPC::grpc_cpp_plugin>
  PLUGIN_OPTIONS generate_mock_code=true
  GENERATE_EXTENSIONS .grpc.pb.h .grpc.pb.cc
  PROTOS
    ${CMAKE_SOURCE_DIR}/proto/kvStore.proto
    ${CMAKE_SOURCE_DIR}/proto/raft.proto
)

target_link_libraries(protobuf_generated PUBLIC
  protobuf::libprotobuf
  gRPC::grpc++
)

target_include_directories(protobuf_generated SYSTEM PUBLIC
  ${CMAKE_BINARY_DIR}
)

# Set position-independent code for protobuf_generated to allow linking into shared libraries
set_target_properties(protobuf_generated PROPERTIES POSITION_INDEPENDENT_CODE ON)

# Enable testing at the root level for proper CTest integration
if(${BUILD_TESTING})
  include(CTest)
  enable_testing()
endif()

add_subdirectory(src)
add_subdirectory(tst)

# Run post-test static analysis: cppcheck and run-clang-tidy.sh
# Usage: cmake --build <build_dir> --target post-test-analysis
add_custom_target(post-test-analysis
  DEPENDS tests
  COMMAND ${CMAKE_COMMAND} -E echo "Running tests with ctest, then post-test static analysis..."
  COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure
)

if(EXISTS "${CMAKE_SOURCE_DIR}/run-cppcheck.sh")
  add_custom_command(TARGET post-test-analysis POST_BUILD
    COMMAND "${CMAKE_SOURCE_DIR}/run-cppcheck.sh" "${CMAKE_BINARY_DIR}" "${CMAKE_SOURCE_DIR}/cppcheck-report.xml"
    COMMENT "Running cppcheck analysis via run-cppcheck.sh"
    VERBATIM
  )
else()
  add_custom_command(TARGET post-test-analysis POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E echo "run-cppcheck.sh not found at ${CMAKE_SOURCE_DIR}, skipping cppcheck."
    VERBATIM
  )
endif()

if(EXISTS "${CMAKE_SOURCE_DIR}/run-clang-tidy.sh")
  add_custom_command(TARGET post-test-analysis POST_BUILD
    COMMAND "${CMAKE_SOURCE_DIR}/run-clang-tidy.sh" "${CMAKE_BINARY_DIR}" "both"
    COMMENT "Running clang-tidy analysis via run-clang-tidy.sh"
    VERBATIM
  )
else()
  add_custom_command(TARGET post-test-analysis POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E echo "run-clang-tidy.sh not found at ${CMAKE_SOURCE_DIR}, skipping."
    VERBATIM
  )
endif()

# --- Installation ---
install(TARGETS zdb
  RUNTIME DESTINATION bin
  LIBRARY DESTINATION lib
  ARCHIVE DESTINATION lib
)

# --- Go Raft Tests ---
# Add Go test targets for Raft implementation
if(${BUILD_TESTING})
  # Test target for TestInitialElection3A
  add_test(NAME raft_test_initial_election_3a
    COMMAND ${CMAKE_COMMAND} -E env
      "LD_LIBRARY_PATH=${CMAKE_BINARY_DIR}/lib"
      go test -run TestInitialElection3A -v
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/6.5840/src/raft1
  )
  
  # Test target for all Raft 3A tests
  add_test(NAME raft_test_3a_all
    COMMAND ${CMAKE_COMMAND} -E env
      "LD_LIBRARY_PATH=${CMAKE_BINARY_DIR}/lib"
      go test -run "Test.*3A" -v
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/6.5840/src/raft1
  )
  
  # Custom target to run just the Raft tests
  add_custom_target(raft_tests
    DEPENDS zdb_raft
    # Build goRaft module first
    COMMAND ${CMAKE_COMMAND} -E echo "Building goRaft module..."
    COMMAND ${CMAKE_COMMAND} -E chdir ${CMAKE_SOURCE_DIR}/src/goRaft
      ${CMAKE_COMMAND} -E env
      "LD_LIBRARY_PATH=${CMAKE_BINARY_DIR}/lib"
      go build ./...
    # Build main 6.5840 module
    COMMAND ${CMAKE_COMMAND} -E echo "Building 6.5840 modules..."
    COMMAND ${CMAKE_COMMAND} -E chdir ${CMAKE_SOURCE_DIR}/6.5840/src
      go build ./raft1/ ./raftapi/ ./tester1/ ./labrpc/ ./labgob/
    # Run the tests
    COMMAND ${CMAKE_COMMAND} -E echo "Running Go Raft tests..."
    COMMAND ${CMAKE_COMMAND} -E chdir ${CMAKE_SOURCE_DIR}/6.5840/src/raft1
      ${CMAKE_COMMAND} -E env
      "LD_LIBRARY_PATH=${CMAKE_BINARY_DIR}/lib"
      go test -run "Test.*3A" -v
    COMMENT "Building Go modules and running Raft tests"
    VERBATIM
  )
  
  # Make the tests depend on the zdb_raft library
  set_tests_properties(raft_test_initial_election_3a PROPERTIES
    DEPENDS zdb_raft
  )
  set_tests_properties(raft_test_3a_all PROPERTIES
    DEPENDS zdb_raft
  )
endif()

# --- Summary ---
message(STATUS "")
message(STATUS "Configuration Summary:")
message(STATUS "  Project: ${PROJECT_NAME} v${PROJECT_VERSION}")
message(STATUS "  Build Type: ${CMAKE_BUILD_TYPE}")
message(STATUS "  C++ Standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "  Build Testing: ${BUILD_TESTING}")
message(STATUS "  Target 'post-test-analysis': runs tests, then cppcheck and run-clang-tidy.sh")
message(STATUS "  Target 'raft_tests': runs Go Raft integration tests")
message(STATUS "")
