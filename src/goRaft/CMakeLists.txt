# Set C++ standard
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Protobuf REQUIRED)
find_package(gRPC REQUIRED)
find_package(spdlog CONFIG REQUIRED)

find_program(GO_BIN go REQUIRED)
find_program(PROTOC_GEN_GO protoc-gen-go REQUIRED)

add_custom_command(
  OUTPUT ${CMAKE_BINARY_DIR}/go-proto/raft.pb.go ${CMAKE_BINARY_DIR}/go-proto/go.mod
  COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/go-proto
  COMMAND ${PROTOC_GEN_GO} --version > /dev/null 2>&1 || echo "protoc-gen-go not found"
  COMMAND ${Protobuf_PROTOC_EXECUTABLE}
      --go_out=${CMAKE_BINARY_DIR}/go-proto
      --go_opt=paths=source_relative
      --proto_path=${CMAKE_SOURCE_DIR}/proto
      --plugin=protoc-gen-go=${PROTOC_GEN_GO}
      ${CMAKE_SOURCE_DIR}/proto/raft.proto
  COMMAND ${CMAKE_COMMAND} -E echo "module proto" > ${CMAKE_BINARY_DIR}/go-proto/go.mod
  COMMAND ${CMAKE_COMMAND} -E echo "" >> ${CMAKE_BINARY_DIR}/go-proto/go.mod
  COMMAND ${CMAKE_COMMAND} -E echo "go 1.23" >> ${CMAKE_BINARY_DIR}/go-proto/go.mod
  COMMAND ${CMAKE_COMMAND} -E echo "" >> ${CMAKE_BINARY_DIR}/go-proto/go.mod
  COMMAND ${CMAKE_COMMAND} -E echo "require google.golang.org/protobuf v1.36.8" >> ${CMAKE_BINARY_DIR}/go-proto/go.mod
  DEPENDS ${CMAKE_SOURCE_DIR}/proto/raft.proto
  COMMENT "Generating Go Protobuf code and module from raft.proto"
)

add_library(zdb_raft SHARED
  ${CMAKE_BINARY_DIR}/go-proto/raft.pb.go
  ${CMAKE_SOURCE_DIR}/src/goRaft/cgo/GoRPCClient.cpp
  ${CMAKE_SOURCE_DIR}/src/goRaft/cgo/raft_wrapper.cpp
  ${CMAKE_SOURCE_DIR}/src/raft/Log.cpp
  ${CMAKE_SOURCE_DIR}/src/raft/SyncChannel.cpp
  ${CMAKE_SOURCE_DIR}/src/raft/AsyncTimer.cpp
  ${CMAKE_SOURCE_DIR}/src/raft/Types.cpp
  ${CMAKE_SOURCE_DIR}/src/common/FullJitter.cpp
  ${CMAKE_SOURCE_DIR}/src/common/RetryPolicy.cpp
  ${CMAKE_SOURCE_DIR}/src/common/ExponentialBackoff.cpp
  ${CMAKE_SOURCE_DIR}/src/common/Util.cpp
  ${CMAKE_SOURCE_DIR}/src/common/CircuitBreaker.cpp
  ${CMAKE_SOURCE_DIR}/src/common/Error.cpp
  ${CMAKE_SOURCE_DIR}/src/common/ErrorConverter.cpp
  ${CMAKE_SOURCE_DIR}/src/common/Repeater.cpp
)

target_include_directories(zdb_raft PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_BINARY_DIR}
)

target_link_libraries(zdb_raft PRIVATE
    spdlog::spdlog
    protobuf_generated
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
  add_custom_target(raft_tests
    DEPENDS zdb_raft
    # Clean and build goRaft module first
    COMMAND ${CMAKE_COMMAND} -E echo "Cleaning and building goRaft module..."
    COMMAND ${CMAKE_COMMAND} -E chdir ${CMAKE_SOURCE_DIR}/src/goRaft
      go clean -cache
    COMMAND ${CMAKE_COMMAND} -E chdir ${CMAKE_SOURCE_DIR}/src/goRaft
      ${CMAKE_COMMAND} -E env
      "LD_LIBRARY_PATH=${CMAKE_BINARY_DIR}/lib"
      go build -a ./...
    # Clean and build main 6.5840 module
    COMMAND ${CMAKE_COMMAND} -E echo "Cleaning and building 6.5840 modules..."
    COMMAND ${CMAKE_COMMAND} -E chdir ${CMAKE_SOURCE_DIR}/6.5840/src
      go clean -cache
    COMMAND ${CMAKE_COMMAND} -E chdir ${CMAKE_SOURCE_DIR}/6.5840/src
      go build -a ./raft1/ ./raftapi/ ./tester1/ ./labrpc/ ./labgob/
    # Run the tests
    COMMAND ${CMAKE_COMMAND} -E echo "Running Go Raft tests..."
    COMMENT "Building Go modules"
    VERBATIM
  )
endif()
