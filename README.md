# ZDB — Distributed KV Store (Z for Zoza!)

ZDB is a learning-oriented distributed key-value store with gRPC, Protobuf, and modern C++.

## Prerequisites

- CMake >= 3.31, Ninja
- One of:
   - GCC 14 (preferred) or GCC 13
   - Clang 18 (uses libc++)
- Git, Bash

## Setup (vcpkg)

We use vcpkg for dependencies.

```bash
./setup-vcpkg.sh
```

This clones and bootstraps vcpkg in `./vcpkg`. CMake presets already point to its toolchain.

## Build

Pick a configure + build preset:

```bash
# GCC 14
cmake --preset=gcc-14-config
cmake --build --preset=gcc-14-build

# GCC 13
cmake --preset=gcc-13-config
cmake --build --preset=gcc-13-build

# LLVM/Clang 18 (libc++)
cmake --preset=llvm-18-config
cmake --build --preset=llvm-18-build
```

Artifacts live under `out/build/<preset>/`.

## Run

The example binary is `zdb`:

```bash
./out/build/gcc-14/zdb
```

The current `main` spins up an in-process gRPC server and exercises a client call; it’s useful for smoke testing.

## Test

Tests are enabled by default and discovered with GoogleTest:

```bash
# After configuring with a preset (e.g., gcc-14-config)
cmake --build --preset=gcc-14-build
ctest --preset=gcc-14-test
```

## Static Analysis (clang-tidy)

Two ways to run:

- From CMake targets (configure with `-DENABLE_CLANG_TIDY=ON` and build, optional), or
- Using the helper script for nicer reports:

```bash
# After a configure so compile_commands.json exists (e.g., out/build/gcc-14)
./run-clang-tidy.sh out/build/gcc-14 html
```

Reports are written to `clang-tidy-reports/` as HTML and/or YAML.

## Project Layout

- `src/` — library and app sources (client, server, common, `zdb/main.cpp`)
- `tst/` — unit tests (GoogleTest)
- `proto/` — Protobuf and gRPC service definitions
- `CMakeLists.txt` — top-level build; generates Protobuf/gRPC sources
- `CMakePresets.json` — presets for GCC/Clang configure, build, and test
- `vcpkg.json` — dependency manifest (gRPC, spdlog, gtest)

## Dependencies

Managed by vcpkg:
- gRPC — RPC framework
- spdlog — structured logging
- GoogleTest — testing framework

## KV Client Usage (C++)

Minimal example showing configuration and basic operations:

```cpp
#include "client/KVStoreClient.hpp"
#include "client/Config.hpp"
#include "common/RetryPolicy.hpp"
#include <iostream>

int main() {
   using namespace std::chrono;
   const std::vector<std::string> peers{ "localhost:50051" };
   zdb::RetryPolicy policy{ 100us, 500ms, 5s, /*servicesToTry=*/3, /*attemptsPerService=*/3 };
   zdb::Config config{ peers, policy };
   zdb::KVStoreClient client{ config };

   if (auto r = client.set("hello", "world"); !r.has_value()) {
      std::cerr << "set failed: " << r.error().what << "\n";
   }

   if (auto g = client.get("hello"); g.has_value()) {
      std::cout << "value: " << g.value() << "\n";
   } else {
      std::cerr << "get failed: " << g.error().what << "\n";
   }

   if (auto s = client.size(); s.has_value()) {
      std::cout << "size: " << s.value() << "\n";
   }

   if (auto e = client.erase("hello"); !e.has_value()) {
      std::cerr << "erase failed: " << e.error().what << "\n";
   }
}
```

Note: The sample `main` currently starts an in-process server for quick smoke tests. For multi-process experiments, you can adapt `src/zdb/main.cpp` to take a port via argv and run multiple instances.

## Architecture (brief)

- Protobuf/gRPC API in `proto/*.proto`; CMake generates C++ stubs into the build tree and exposes them via the `protobuf_generated` static library.
- Server:
   - `KVStoreServiceImpl` implements RPCs (get/set/erase/size) over an `InMemoryKVStore`.
   - `KVStoreServer` wraps `grpc::Server` setup and lifecycle (listen, wait, shutdown).
- Client:
   - `KVStoreClient` exposes a simple API (get/set/erase/size) returning `std::expected` values.
   - `KVRPCService` owns a gRPC stub per peer and applies a `CircuitBreaker`.
   - `Config` manages peer set and selection; integrates `RetryPolicy` with backoff/jitter via utilities in `common/` (ExponentialBackoff, FullJitter, Repeater).
- Common:
   - `Error`/`ErrorConverter` map between internal errors and gRPC status codes.

### Proto generation

Editing files in `proto/` requires no manual steps—rebuilding regenerates sources. Both protobuf messages and gRPC services are generated (with mock code) by CMake using `protobuf_generate`.

## CMake Options

- `-DENABLE_CLANG_TIDY=ON` — enable clang-tidy during builds (requires clang-tidy installed).
- `-DENABLE_CPPCHECK=ON` — enable cppcheck during builds if configured in your environment.
- `-DBUILD_TESTING=ON` — enabled by default; needed for building tests.

## License

See `LICENSE`.
