# SPDX-License-Identifier: AGPL-3.0-or-later
# ZDB a distributed, fault-tolerant database.
# Copyright (C) 2025 Ahmed Refaat Gadalla Mohamed
#
# This program is free software: you can redistribute it and/or modify it under the terms of the GNU Affero General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Affero General Public License for more details.
#
# You should have received a copy of the GNU Affero General Public License along with this program. If not, see <https://www.gnu.org/licenses/>.

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Protobuf REQUIRED)
find_package(gRPC REQUIRED)
find_package(spdlog CONFIG REQUIRED)

find_program(GO_BIN go REQUIRED)
find_program(PROTOC_GEN_GO protoc-gen-go REQUIRED)
set(PROTO_BUILD_DIR "${CMAKE_BINARY_DIR}/go-proto")
add_custom_command(
  OUTPUT ${PROTO_BUILD_DIR}/raft.pb.go ${PROTO_BUILD_DIR}/types.pb.go ${PROTO_BUILD_DIR}/go.mod
  COMMAND ${CMAKE_COMMAND} -E make_directory ${PROTO_BUILD_DIR}
  COMMAND ${PROTOC_GEN_GO} --version > /dev/null 2>&1 || echo "protoc-gen-go not found"
  COMMAND ${Protobuf_PROTOC_EXECUTABLE}
      --go_out=${PROTO_BUILD_DIR}
      --go_opt=paths=source_relative
      --proto_path=${CMAKE_SOURCE_DIR}/proto
      --plugin=protoc-gen-go=${PROTOC_GEN_GO}
      ${CMAKE_SOURCE_DIR}/proto/raft.proto ${CMAKE_SOURCE_DIR}/proto/types.proto
  COMMAND ${CMAKE_COMMAND} -E echo "module proto" > ${PROTO_BUILD_DIR}/go.mod
  COMMAND ${CMAKE_COMMAND} -E echo "" >> ${PROTO_BUILD_DIR}/go.mod
  COMMAND ${CMAKE_COMMAND} -E echo "go 1.23" >> ${PROTO_BUILD_DIR}/go.mod
  COMMAND ${CMAKE_COMMAND} -E echo "" >> ${PROTO_BUILD_DIR}/go.mod
  COMMAND ${CMAKE_COMMAND} -E echo "require google.golang.org/protobuf v1.36.8" >> ${PROTO_BUILD_DIR}/go.mod
  DEPENDS ${CMAKE_SOURCE_DIR}/proto/raft.proto ${CMAKE_SOURCE_DIR}/proto/types.proto
  COMMENT "Generating Go Protobuf code and module from raft.proto"
)

# Create a target that depends on the protobuf generation
add_custom_target(go_proto_gen
  DEPENDS ${PROTO_BUILD_DIR}/raft.pb.go ${PROTO_BUILD_DIR}/types.pb.go ${PROTO_BUILD_DIR}/go.mod
)

set(RAFT_SRC_DIR "${CMAKE_SOURCE_DIR}/6.5840/src")

# Generate go.mod from template
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/go.mod.in"
    "${CMAKE_CURRENT_SOURCE_DIR}/go.mod"
    @ONLY
)


add_library(zdb_raft SHARED
  ${CMAKE_SOURCE_DIR}/src/goRaft/cgo/GoRPCClient.cpp
  ${CMAKE_SOURCE_DIR}/src/goRaft/cgo/raft_wrapper.cpp
  ${CMAKE_SOURCE_DIR}/src/raft/Log.cpp
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
  ${CMAKE_SOURCE_DIR}/src/common/KVStateMachine.cpp
  ${CMAKE_SOURCE_DIR}/src/common/Command.cpp
  ${CMAKE_SOURCE_DIR}/src/common/Types.cpp
  ${CMAKE_SOURCE_DIR}/src/goRaft/cgo/GoChannel.cpp
  ${CMAKE_SOURCE_DIR}/src/goRaft/cgo/GoPersister.cpp
)

target_compile_options(zdb_raft PRIVATE
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

    -g
    -Og
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
    DEPENDS zdb_raft ${CMAKE_CURRENT_SOURCE_DIR}/go.mod ${PROTO_BUILD_DIR}/raft.pb.go ${PROTO_BUILD_DIR}/types.pb.go ${PROTO_BUILD_DIR}/go.mod
    # Clean and build goRaft module first
    COMMAND ${CMAKE_COMMAND} -E echo "Cleaning and building goRaft module..."
    COMMAND ${CMAKE_COMMAND} -E chdir ${CMAKE_SOURCE_DIR}/src/goRaft
      go clean -cache
    COMMAND ${CMAKE_COMMAND} -E echo "replace github.com/as-family/zdb/proto => ${PROTO_BUILD_DIR}" >> ${CMAKE_SOURCE_DIR}/src/goRaft/go.mod
    COMMAND ${CMAKE_COMMAND} -E chdir ${CMAKE_SOURCE_DIR}/src/goRaft
      ${CMAKE_COMMAND} -E env
      "LD_LIBRARY_PATH=${CMAKE_BINARY_DIR}/lib"
      go build -a ./...
    # Clean and build main 6.5840 module
    COMMAND ${CMAKE_COMMAND} -E echo "Cleaning and building 6.5840 modules..."
    COMMAND ${CMAKE_COMMAND} -E chdir ${CMAKE_SOURCE_DIR}/6.5840/src
      go clean -cache
    COMMAND ${CMAKE_COMMAND} -E echo "replace github.com/as-family/zdb/proto => ${PROTO_BUILD_DIR}" >> ${CMAKE_SOURCE_DIR}/6.5840/src/go.mod
    COMMAND ${CMAKE_COMMAND} -E chdir ${CMAKE_SOURCE_DIR}/6.5840/src
      go build -a ./raft1/ ./raftapi/ ./tester1/ ./labrpc/ ./labgob/
    COMMENT "Building Go modules"
    VERBATIM
  )

  add_test(
    NAME raft_3a_tests
    COMMAND ${CMAKE_COMMAND} -E env "LD_LIBRARY_PATH=${CMAKE_BINARY_DIR}/lib"
            ${GO_BIN} test -run "Test.*3A" -v
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/6.5840/src/raft1
  )

  set_tests_properties(raft_3a_tests PROPERTIES
    ENVIRONMENT "LD_LIBRARY_PATH=${CMAKE_BINARY_DIR}/lib"
    FIXTURES_REQUIRED "raft_setup"
  )

  add_test(
          NAME raft_3b_tests
          COMMAND ${CMAKE_COMMAND} -E env "LD_LIBRARY_PATH=${CMAKE_BINARY_DIR}/lib"
          ${GO_BIN} test -run "Test.*3B" -v
          WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/6.5840/src/raft1
  )

  set_tests_properties(raft_3b_tests PROPERTIES
          ENVIRONMENT "LD_LIBRARY_PATH=${CMAKE_BINARY_DIR}/lib"
          FIXTURES_REQUIRED "raft_setup"
  )

  add_test(
          NAME raft_3c_tests
          COMMAND ${CMAKE_COMMAND} -E env "LD_LIBRARY_PATH=${CMAKE_BINARY_DIR}/lib"
          ${GO_BIN} test -run "Test.*3C" -v
          WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/6.5840/src/raft1
  )

  set_tests_properties(raft_3c_tests PROPERTIES
          ENVIRONMENT "LD_LIBRARY_PATH=${CMAKE_BINARY_DIR}/lib"
          FIXTURES_REQUIRED "raft_setup"
  )
  
  add_test(
    NAME raft_setup
    COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR} --target zdb_raft --target go_proto_gen
  )
  
  set_tests_properties(raft_setup PROPERTIES
    FIXTURES_SETUP "raft_setup"
  )
endif()
