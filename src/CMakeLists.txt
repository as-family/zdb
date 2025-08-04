add_executable(zdb)

target_include_directories(zdb PRIVATE
  ${CMAKE_SOURCE_DIR}/src
)
target_include_directories(zdb PRIVATE
  ${CMAKE_BINARY_DIR}
)

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
  ${CMAKE_SOURCE_DIR}/src/zdb/main.cpp
  ${CMAKE_SOURCE_DIR}/src/zdb/PeerNode.cpp
  ${CMAKE_SOURCE_DIR}/src/server/InMemoryKVStore.cpp
  ${CMAKE_SOURCE_DIR}/src/server/KVStoreServer.cpp
  ${CMAKE_SOURCE_DIR}/src/client/KVStoreClient.cpp
  ${CMAKE_SOURCE_DIR}/src/client/KVRPCService.cpp
  ${CMAKE_SOURCE_DIR}/src/common/CircuitBreaker.cpp
  ${CMAKE_SOURCE_DIR}/src/common/Error.cpp
  ${CMAKE_SOURCE_DIR}/src/common/ErrorConverter.cpp
  ${CMAKE_SOURCE_DIR}/src/common/ExponentialBackoff.cpp
  ${CMAKE_SOURCE_DIR}/src/common/FullJitter.cpp
  ${CMAKE_SOURCE_DIR}/src/common/Repeater.cpp
  ${CMAKE_SOURCE_DIR}/src/common/RetryPolicy.cpp
  ${CMAKE_SOURCE_DIR}/src/client/Config.cpp
)

target_link_libraries(zdb PRIVATE
  protobuf::libprotobuf
  grpc++
  spdlog::spdlog
  protobuf_generated
)
