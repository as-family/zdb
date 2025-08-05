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

# Static analysis options
option(ENABLE_CLANG_TIDY "Enable clang-tidy static analysis" OFF)
option(ENABLE_CPPCHECK "Enable cppcheck static analysis" OFF)

# Configure clang-tidy
if(ENABLE_CLANG_TIDY)
  find_program(CLANG_TIDY_EXE NAMES "clang-tidy" DOC "Path to clang-tidy executable")
  if(NOT CLANG_TIDY_EXE)
    message(STATUS "clang-tidy not found.")
  else()
    message(STATUS "clang-tidy found: ${CLANG_TIDY_EXE}")
    set(CMAKE_CXX_CLANG_TIDY "${CLANG_TIDY_EXE}")
  endif()
endif()

set(BUILD_TESTING ON)

include(CTest)

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

# Add clang-tidy target for manual execution
if(CLANG_TIDY_EXE)
  file(GLOB_RECURSE ALL_SOURCE_FILES 
    ${CMAKE_SOURCE_DIR}/src/*.cpp 
    ${CMAKE_SOURCE_DIR}/src/*.hpp
  )
  
  add_custom_target(clang-tidy
    COMMAND ${CLANG_TIDY_EXE}
    -p ${CMAKE_BINARY_DIR}
    --format-style=file
    --quiet
    ${ALL_SOURCE_FILES}
    COMMENT "Running clang-tidy on source files"
    VERBATIM
  )
endif()

# Add run-clang-tidy target for parallel execution (if available)
find_program(RUN_CLANG_TIDY_EXE NAMES "run-clang-tidy" DOC "Path to run-clang-tidy script")
if(RUN_CLANG_TIDY_EXE)
  add_custom_target(run-clang-tidy
    COMMAND ${RUN_CLANG_TIDY_EXE}
    -p ${CMAKE_BINARY_DIR}
    -quiet
    ${CMAKE_SOURCE_DIR}/src/
    COMMENT "Running clang-tidy in parallel on source files"
    VERBATIM
  )
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
message(STATUS "")
