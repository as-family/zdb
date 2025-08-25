if(${BUILD_TESTING})
  include(CTest)

  find_package(GTest CONFIG REQUIRED)
  find_package(nlohmann_json CONFIG REQUIRED)
  include(GoogleTest)

  add_executable(tests)
  enable_testing()

  target_include_directories(tests PRIVATE
    ${CMAKE_SOURCE_DIR}/src
  )
    target_include_directories(tests PRIVATE
    ${CMAKE_SOURCE_DIR}/tst
  )
  target_include_directories(tests PRIVATE
    ${CMAKE_BINARY_DIR}
  )

  target_sources(tests PRIVATE
    ${CMAKE_SOURCE_DIR}/src/common/ExponentialBackoff.cpp
    ${CMAKE_SOURCE_DIR}/src/common/RetryPolicy.cpp
    ${CMAKE_SOURCE_DIR}/src/common/FullJitter.cpp
    ${CMAKE_SOURCE_DIR}/src/common/Error.cpp
    ${CMAKE_SOURCE_DIR}/src/common/ErrorConverter.cpp
    ${CMAKE_SOURCE_DIR}/src/common/Repeater.cpp
    ${CMAKE_SOURCE_DIR}/src/common/CircuitBreaker.cpp
    ${CMAKE_SOURCE_DIR}/src/client/KVStoreClient.cpp
    ${CMAKE_SOURCE_DIR}/src/server/InMemoryKVStore.cpp
    ${CMAKE_SOURCE_DIR}/src/server/KVStoreServer.cpp
    ${CMAKE_SOURCE_DIR}/src/client/Config.cpp
    ${CMAKE_SOURCE_DIR}/src/common/Types.cpp
    ${CMAKE_SOURCE_DIR}/src/lock/Lock.cpp
    ${CMAKE_SOURCE_DIR}/src/raft/RaftImpl.cpp
    ${CMAKE_SOURCE_DIR}/src/raft/Channel.cpp
    ${CMAKE_SOURCE_DIR}/src/raft/RaftServiceImpl.cpp
    ${CMAKE_SOURCE_DIR}/src/raft/Log.cpp
    ${CMAKE_SOURCE_DIR}/src/raft/Types.cpp
    ${CMAKE_SOURCE_DIR}/src/raft/AsyncTimer.cpp
    ${CMAKE_SOURCE_DIR}/tst/server/InMemoryKVStoreTest.cpp
    ${CMAKE_SOURCE_DIR}/tst/common/ExponentialBackoffTest.cpp
    ${CMAKE_SOURCE_DIR}/tst/common/RetryPolicyTest.cpp
    ${CMAKE_SOURCE_DIR}/tst/common/FullJitterTest.cpp
    ${CMAKE_SOURCE_DIR}/tst/common/ErrorConverterTest.cpp
    ${CMAKE_SOURCE_DIR}/tst/common/RepeaterTest.cpp
    ${CMAKE_SOURCE_DIR}/tst/common/CircuitBreakerTest.cpp
    ${CMAKE_SOURCE_DIR}/tst/client/KVRPCServiceTest.cpp
    ${CMAKE_SOURCE_DIR}/tst/server/KVStoreServerTest.cpp
    ${CMAKE_SOURCE_DIR}/tst/client/KVStoreClientTest.cpp
    ${CMAKE_SOURCE_DIR}/tst/client/ConfigTest.cpp
    ${CMAKE_SOURCE_DIR}/tst/KVTestFramework/KVTestFramework.cpp
    ${CMAKE_SOURCE_DIR}/tst/KVTestFramework/ProxyKVStoreService.cpp
    ${CMAKE_SOURCE_DIR}/tst/KVTestFramework/NetworkConfig.cpp
    ${CMAKE_SOURCE_DIR}/tst/kvTest1/KVTest.cpp
    ${CMAKE_SOURCE_DIR}/tst/KVTestFramework/KVTestFrameworkTest.cpp
    ${CMAKE_SOURCE_DIR}/tst/KVTestFramework/Porcupine.cpp
    ${CMAKE_SOURCE_DIR}/tst/kvTest1/LockTest.cpp
    ${CMAKE_SOURCE_DIR}/tst/raftTest/RaftTest.cpp
  )

  target_link_libraries(tests PRIVATE
    GTest::gtest_main
    GTest::gmock_main
    gRPC::grpc++
    protobuf::libprotobuf
    protobuf_generated
    spdlog::spdlog
    nlohmann_json::nlohmann_json
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

  # Build the Go porcupine checker
  find_program(GO_EXECUTABLE go)
  if(GO_EXECUTABLE)
    add_custom_command(
      OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/porcupine
      COMMAND ${GO_EXECUTABLE} build -o ${CMAKE_CURRENT_BINARY_DIR}/porcupine
      WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/tst/KVTestFramework/porcupine
      DEPENDS 
        ${CMAKE_SOURCE_DIR}/tst/KVTestFramework/porcupine/main.go
        ${CMAKE_SOURCE_DIR}/tst/KVTestFramework/porcupine/go.mod
        ${CMAKE_SOURCE_DIR}/tst/KVTestFramework/porcupine/go.sum
      COMMENT "Building Go porcupine checker"
      VERBATIM
    )

    add_custom_target(porcupine_checker ALL
      DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/porcupine
    )

    add_dependencies(tests porcupine_checker)
  else()
    message(WARNING "Go not found. Porcupine checker will not be built.")
  endif()

  gtest_discover_tests(tests)
endif()
