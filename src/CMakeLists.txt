add_executable(zdb)

target_include_directories(zdb PRIVATE
  ${CMAKE_SOURCE_DIR}/src
)
target_include_directories(zdb PRIVATE
  ${CMAKE_BINARY_DIR}
)

target_compile_options(zdb PRIVATE
  -Wall
  -Wextra
  -Wpedantic

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

# Add debug flags only for Debug and RelWithDebInfo builds
target_compile_options(zdb PRIVATE
  $<$<CONFIG:Debug>:-g -Og>
  $<$<CONFIG:RelWithDebInfo>:-g>
)
target_compile_features(zdb PRIVATE cxx_std_23)

target_sources(zdb PRIVATE
  ${CMAKE_SOURCE_DIR}/src/zdb/main.cpp
  ${CMAKE_SOURCE_DIR}/src/storage/InMemoryKVStore.cpp
  ${CMAKE_SOURCE_DIR}/src/server/KVStoreServiceImpl.cpp
  ${CMAKE_SOURCE_DIR}/src/client/KVStoreClient.cpp
  ${CMAKE_SOURCE_DIR}/src/common/CircuitBreaker.cpp
  ${CMAKE_SOURCE_DIR}/src/common/Error.cpp
  ${CMAKE_SOURCE_DIR}/src/common/ErrorConverter.cpp
  ${CMAKE_SOURCE_DIR}/src/common/ExponentialBackoff.cpp
  ${CMAKE_SOURCE_DIR}/src/common/FullJitter.cpp
  ${CMAKE_SOURCE_DIR}/src/common/Repeater.cpp
  ${CMAKE_SOURCE_DIR}/src/common/RetryPolicy.cpp
  ${CMAKE_SOURCE_DIR}/src/client/Config.cpp
  ${CMAKE_SOURCE_DIR}/src/common/Types.cpp
  ${CMAKE_SOURCE_DIR}/src/lock/Lock.cpp
  ${CMAKE_SOURCE_DIR}/src/raft/RaftServiceImpl.cpp
  ${CMAKE_SOURCE_DIR}/src/raft/Log.cpp
  ${CMAKE_SOURCE_DIR}/src/raft/Types.cpp
  ${CMAKE_SOURCE_DIR}/src/raft/AsyncTimer.cpp
  ${CMAKE_SOURCE_DIR}/src/common/Util.cpp
  ${CMAKE_SOURCE_DIR}/src/common/Command.cpp
  ${CMAKE_SOURCE_DIR}/src/common/KVStateMachine.cpp
  ${CMAKE_SOURCE_DIR}/src/raft/SyncChannel.cpp
)

target_link_libraries(zdb PRIVATE
  protobuf::libprotobuf
  gRPC::grpc++
  spdlog::spdlog
  protobuf_generated
)
