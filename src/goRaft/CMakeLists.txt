cmake_minimum_required(VERSION 3.26)

# Set C++ standard
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find required packages
find_package(Protobuf REQUIRED)
find_package(gRPC REQUIRED)
find_package(spdlog CONFIG REQUIRED)

# Create the shared library for Go CGO
add_library(zdb_raft SHARED
    cgo/raft_wrapper.cpp
    cgo/labrpc_client.cpp
    ../raft/RaftImpl.hpp
    ../raft/Log.cpp
    ../raft/RaftServiceImpl.cpp
    ../raft/SyncChannel.cpp
    ../raft/AsyncTimer.cpp
    ../raft/Types.cpp
    ../common/FullJitter.cpp
    ../common/RetryPolicy.cpp
    ../common/ExponentialBackoff.cpp
    ../common/Util.cpp
    ../common/CircuitBreaker.cpp
    ../common/Error.cpp
    ../common/ErrorConverter.cpp
    ../common/Repeater.cpp
)

target_include_directories(zdb_raft PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/out/build/gcc-14
)

target_link_libraries(zdb_raft PRIVATE
    protobuf_generated
    spdlog::spdlog
)

# Ensure position-independent code for shared library
set_target_properties(zdb_raft PROPERTIES
    POSITION_INDEPENDENT_CODE ON
    CXX_STANDARD 23
)

# Copy to lib directory for easier linking
add_custom_command(TARGET zdb_raft POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/lib/
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    $<TARGET_FILE:zdb_raft>
    ${CMAKE_BINARY_DIR}/lib/
)

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
