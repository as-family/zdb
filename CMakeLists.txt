cmake_minimum_required(VERSION 4.0.0)

set(CMAKE_EXPERIMENTAL_CXX_IMPORT_STD "a9e1cf81-9932-4810-974b-6eccaf14e457")

project(zdb
  VERSION 1.0.0
  DESCRIPTION "ZDB Z for Zoza"
  LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# C++ Modules support (experimental)
set(CMAKE_CXX_MODULE_STD ON)
set(CMAKE_CXX_SCAN_FOR_MODULES ON)

# --- Options ---
option(BUILD_TESTING "Build tests" ON)
option(BUILD_SHARED_LIBS "Build shared libraries" ON)

include(FetchContent)
include(CTest)

set(FETCHCONTENT_QUIET ON)

FetchContent_Declare(
  abseil-cpp
  GIT_REPOSITORY https://github.com/abseil/abseil-cpp.git
  GIT_TAG 20250127.1
  FIND_PACKAGE_ARGS
)
set(ABSL_PROPAGATE_CXX_STD ON CACHE BOOL "Propagate CXX standard" FORCE)
set(ABSL_BUILD_TESTING OFF CACHE BOOL "Disable Abseil tests" FORCE)
set(ABSL_USE_EXTERNAL_GOOGLETEST ON CACHE BOOL "Use external GoogleTest" FORCE)
set(ABSL_ENABLE_INSTALL ON)
FetchContent_MakeAvailable(abseil-cpp)

FetchContent_Declare(
  protobuf
  GIT_REPOSITORY https://github.com/protocolbuffers/protobuf.git
  GIT_TAG v31.0
  FIND_PACKAGE_ARGS
)
set(protobuf_BUILD_TESTS OFF CACHE BOOL "Build protobuf tests" FORCE)
set(protobuf_WITH_ZLIB OFF CACHE BOOL "Build with zlib support" FORCE)
set(protobuf_BUILD_SHARED_LIBS ${BUILD_SHARED_LIBS} CACHE BOOL "Build shared protobuf libraries" FORCE)
set(protobuf_ABSL_PROVIDER "package" CACHE STRING "Use Abseil from package" FORCE)
set(protobuf_BUILD_LIBUPB OFF CACHE BOOL "Build libupb" FORCE)
FetchContent_MakeAvailable(protobuf)

FetchContent_Declare(
  grpc
  GIT_REPOSITORY https://github.com/grpc/grpc.git
  GIT_TAG v1.73.1
  FIND_PACKAGE_ARGS
)
set(gRPC_BUILD_TESTS OFF CACHE BOOL "Build gRPC tests" FORCE)
set(gRPC_INSTALL ON CACHE BOOL "Install gRPC" FORCE)
set(gRPC_PROTOBUF_PROVIDER "package" CACHE STRING "Use Protobuf from package" FORCE)
set(gRPC_ABSL_PROVIDER "package" CACHE STRING "Use Abseil from package" FORCE)
set(gRPC_BUILD_GRPC_CPP_PLUGIN ON CACHE BOOL "Build grpc_cpp_plugin" FORCE)
FetchContent_MakeAvailable(grpc)

FetchContent_Declare(
  spdlog
  GIT_REPOSITORY https://github.com/gabime/spdlog.git
  GIT_TAG v1.15.3
  FIND_PACKAGE_ARGS
)
set(SPDLOG_BUILD_EXAMPLE OFF CACHE BOOL "Build spdlog examples" FORCE)
set(SPDLOG_BUILD_TESTS OFF CACHE BOOL "Build spdlog tests" FORCE)
FetchContent_MakeAvailable(spdlog)

add_executable(zdb)
target_compile_options(zdb PRIVATE
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

  -fsanitize=address
  -fsanitize=undefined
  # -fsanitize=thread
  -fstack-protector-strong
  -D_FORTIFY_SOURCE=2
)

target_link_options(zdb PRIVATE
  -fsanitize=address
  -fsanitize=undefined
)

target_sources(zdb
  PRIVATE
    src/main.cpp
)

target_sources(zdb
  PRIVATE
    FILE_SET CXX_MODULES
    FILES
      src/example.cppm
)

target_link_libraries(zdb
  PRIVATE
    protobuf::libprotobuf
    grpc++
    spdlog::spdlog
)


# --- Protocol Buffers Generation ---
# Uncomment and modify if you have .proto files
# find_program(PROTOC_EXECUTABLE protoc REQUIRED)
# find_program(GRPC_CPP_PLUGIN grpc_cpp_plugin REQUIRED)
#
# function(generate_proto_sources PROTO_FILES)
#   foreach(PROTO_FILE ${PROTO_FILES})
#     get_filename_component(PROTO_NAME ${PROTO_FILE} NAME_WE)
#     get_filename_component(PROTO_DIR ${PROTO_FILE} DIRECTORY)
#
#     set(PROTO_SRCS "${CMAKE_CURRENT_BINARY_DIR}/${PROTO_NAME}.pb.cc")
#     set(PROTO_HDRS "${CMAKE_CURRENT_BINARY_DIR}/${PROTO_NAME}.pb.h")
#     set(GRPC_SRCS "${CMAKE_CURRENT_BINARY_DIR}/${PROTO_NAME}.grpc.pb.cc")
#     set(GRPC_HDRS "${CMAKE_CURRENT_BINARY_DIR}/${PROTO_NAME}.grpc.pb.h")
#
#     add_custom_command(
#       OUTPUT ${PROTO_SRCS} ${PROTO_HDRS} ${GRPC_SRCS} ${GRPC_HDRS}
#       COMMAND ${PROTOC_EXECUTABLE}
#       ARGS --cpp_out=${CMAKE_CURRENT_BINARY_DIR}
#            --grpc_out=${CMAKE_CURRENT_BINARY_DIR}
#            --plugin=protoc-gen-grpc=${GRPC_CPP_PLUGIN}
#            -I${CMAKE_CURRENT_SOURCE_DIR}/${PROTO_DIR}
#            ${CMAKE_CURRENT_SOURCE_DIR}/${PROTO_FILE}
#       DEPENDS ${PROTO_FILE}
#       COMMENT "Generating protobuf and gRPC sources for ${PROTO_FILE}"
#     )
#
#     target_sources(zdb PRIVATE ${PROTO_SRCS} ${GRPC_SRCS})
#     target_include_directories(zdb PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
#   endforeach()
# endfunction()
#
# # Example usage:
# # generate_proto_sources("src/proto/example.proto")

# --- Testing ---
if(BUILD_TESTING)
  FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG v1.17.0
    FIND_PACKAGE_ARGS
  )

  # For Windows: Prevent overriding parent project's compiler/linker settings
  set(gtest_force_shared_crt ON CACHE BOOL "Force shared CRT" FORCE)
  set(BUILD_GMOCK ON CACHE BOOL "Build GMock" FORCE)
  set(INSTALL_GTEST OFF CACHE BOOL "Install GTest" FORCE)

  FetchContent_MakeAvailable(googletest)
  include(GoogleTest)

  # Only create test target if test files exist
  if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/unit_tests.cpp")
    add_executable(unit_tests)

    target_sources(unit_tests
      PRIVATE
        src/unit_tests.cpp
    )

    target_link_libraries(unit_tests
      PRIVATE
        GTest::gtest_main
        GTest::gmock_main
        protobuf::libprotobuf
        grpc++
        spdlog::spdlog
    )

    set_target_properties(unit_tests PROPERTIES
      CXX_STANDARD 23
      CXX_STANDARD_REQUIRED ON
      CXX_EXTENSIONS OFF
    )

    gtest_discover_tests(unit_tests)
  else()
    message(WARNING "unit_tests.cpp not found, skipping test target creation")
  endif()
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
message(STATUS "  Build Shared Libraries: ${BUILD_SHARED_LIBS}")
message(STATUS "")
