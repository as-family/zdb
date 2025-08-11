if(${BUILD_TESTING})
  include(CTest)

  find_package(GTest CONFIG REQUIRED)
  include(GoogleTest)

  add_executable(tests)
  enable_testing()

  target_include_directories(tests PRIVATE
    ${CMAKE_SOURCE_DIR}/src
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
    ${CMAKE_SOURCE_DIR}/src/client/KVRPCService.cpp
    ${CMAKE_SOURCE_DIR}/src/client/KVStoreClient.cpp
    ${CMAKE_SOURCE_DIR}/src/server/InMemoryKVStore.cpp
    ${CMAKE_SOURCE_DIR}/src/server/KVStoreServer.cpp
    ${CMAKE_SOURCE_DIR}/src/client/Config.cpp
    ${CMAKE_SOURCE_DIR}/src/common/Types.cpp
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
    ${CMAKE_SOURCE_DIR}/tst/kvTest1/KVTestFramework.cpp
    ${CMAKE_SOURCE_DIR}/tst/kvTest1/TestKVServer.cpp
  )

  target_link_libraries(tests PRIVATE
    GTest::gtest_main
    GTest::gmock_main
    gRPC::grpc++
    protobuf::libprotobuf
    protobuf_generated
    spdlog::spdlog
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

  gtest_discover_tests(tests)
endif()
