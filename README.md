# zDB: A High-Performance, Raft-Based, Sharded Key-Value Store in C++

[![Release](https://github.com/as-family/zdb/actions/workflows/cmake-single-platform.yml/badge.svg?branch=master)](https://github.com/as-family/zdb/actions/workflows/cmake-single-platform.yml)
[![License: AGPL-3.0](https://img.shields.io/badge/License-AGPL--3.0-blue.svg)](https://www.gnu.org/licenses/agpl-3.0)
[![C++23](https://img.shields.io/badge/C%2B%2B-23-blue.svg)](https://en.cppreference.com/w/cpp/23)

## Overview

zDB is a distributed, fault-tolerant key-value store implemented in modern C++23, based on the architecture described in the MIT 6.824 Distributed Systems course. It implements sharded replication using the Raft consensus protocol to provide strong consistency and fault tolerance. The implementation successfully passes the rigorous test suite provided by the course while extending the functionality with modern C++ features and performance optimizations.

## Key Features

- **Fault-Tolerant Consensus**
  - Full implementation of the Raft protocol
  - Leader election with heartbeat mechanism
  - Log replication with consistency checks
  - Persistent state and log storage

- **High Availability**
  - Automatic leader election on failures
  - Seamless failover between replicas
  - Log compaction via snapshotting

- **Strong Consistency**
  - Linearizable Get/Put/Append operations
  - Atomic multi-key operations within shards
  - Consistent reads across replicas

## Quick Start

### Prerequisites
- Modern C++ compiler with C++23 support
- CMake 3.31 or higher

### Building from Source

1. Clone the repository:
   ```bash
   git clone https://github.com/as-family/zdb.git
   cd zdb
   ```

2. Set up vcpkg dependencies:
   ```bash
   ./setup-vcpkg.sh
   ```

3. Build the project:
   ```bash
   cmake --preset sys-gcc
   cmake --build --preset sys-gcc
   ```

4. Run tests:
   ```bash
   ctest --preset sys-gcc
   ```

## Documentation

- [Architecture & Design Document](docs/DESIGN.md) - Detailed discussion of technical decisions and implementation details
- [Raft Protocol Details](https://raft.github.io/) - Extended Raft consensus algorithm documentation

## Roadmap
- [ ] **Sharding & Scalability**
  - [ ] Dynamic shard allocation and rebalancing
  - [ ] Automatic shard migration
  - [ ] Linear scaling with number of shards
  - [ ] Load-balanced key distribution

- [ ] Multi-key ACID transactions across shards
- [ ] Read-only Ops via leader leases for improved read scalability
- [ ] Pluggable storage engines
- [ ] Metrics and monitoring integration
- [ ] Client libraries in multiple languages
- [ ] Dynamic membership changes

## License

zDB is licensed under the GNU Affero General Public License v3.0. See the [LICENSE](LICENSE) file for details.
