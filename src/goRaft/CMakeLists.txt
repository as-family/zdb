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
)

# Set properties for the library
set_target_properties(zdb_raft PROPERTIES
    CXX_STANDARD 23
    CXX_STANDARD_REQUIRED ON
    POSITION_INDEPENDENT_CODE ON
)

# Include directories
target_include_directories(zdb_raft PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/cgo
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_BINARY_DIR}  # For generated protobuf files
)

# Link libraries
target_link_libraries(zdb_raft
    protobuf::libprotobuf
    gRPC::grpc++
    spdlog::spdlog
    protobuf_generated  # Add dependency on generated protobuf files
    pthread
)

# Compiler-specific options
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_compile_options(zdb_raft PRIVATE
        -Wall
        -Wextra
        -fPIC
    )
endif()

# Set output directory
set_target_properties(zdb_raft PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

# Install rules
install(TARGETS zdb_raft
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin
)
