if(${BUILD_TESTING})
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
    ${CMAKE_SOURCE_DIR}/tst/InMemoryKVStoreTest.cpp
    ${CMAKE_SOURCE_DIR}/tst/ExponentialBackoffTest.cpp
    ${CMAKE_SOURCE_DIR}/tst/RetryPolicyTest.cpp
    ${CMAKE_SOURCE_DIR}/tst/FullJitterTest.cpp
    ${CMAKE_SOURCE_DIR}/tst/ErrorConverterTest.cpp
    ${CMAKE_SOURCE_DIR}/tst/RepeaterTest.cpp
    ${CMAKE_SOURCE_DIR}/tst/CircuitBreakerTest.cpp
    ${CMAKE_SOURCE_DIR}/tst/KVRPCServiceTest.cpp
    ${CMAKE_SOURCE_DIR}/tst/KVStoreServerTest.cpp
    ${CMAKE_SOURCE_DIR}/tst/KVStoreClientTest.cpp
  )

  target_link_libraries(tests PRIVATE
    GTest::gtest_main
    GTest::gmock_main
    grpc++
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
