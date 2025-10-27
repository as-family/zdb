# zDB Technical Design Document

This document outlines the key technical decisions and architectural choices made in the implementation of zDB.

## System Architecture
![arch](figures/arch.png)
### Core Components

1. **Client Interface**
   - gRPC-based API for client interactions
   - Asynchronous operation support
   - Retry mechanisms

2. **Shard Master**
   - Manages shard assignments and reconfigurations
   - Maintains global view of system topology
   - Handles shard rebalancing decisions

3. **Raft Groups**
   - Independent Raft clusters per shard
   - Leader-based consensus protocol
   - Persistent state management

### Key Technical Decisions

#### RPC Framework: gRPC
- **Why gRPC?**
  - Native C++ support with high performance
  - Built-in streaming capabilities
  - Strong type safety with Protocol Buffers
  - Extensive middleware support

#### State Persistence
- **Log Storage**
  - Structured log format with checksums
  - Batched disk writes for performance
  - Memory-mapped files for fast access
  
- **Snapshotting**
  - Incremental snapshot strategy
  - Background compaction process
  - Copy-on-write for concurrent access

#### Concurrency Model
- Lock-free data structures where possible
- Fine-grained locking for state updates
- Thread pool for request processing
- Asynchronous I/O operations

#### Sharding Strategy
- Consistent hashing for key distribution
- Virtual nodes for better balance
- Dynamic shard splitting and merging
- Background rebalancing

### Performance Optimizations

1. **Request Batching**
   - Batch client requests for higher throughput
   - Coalesce disk writes
   - Group commit optimization

2. **Memory Management**
   - Custom memory allocators
   - Memory pooling for common objects
   - Zero-copy operations where possible

3. **Network Optimizations**
   - Connection pooling
   - Protocol-level compression
   - Efficient serialization

## Implementation Challenges

### Raft Implementation
- Careful handling of edge cases in leader election
- Proper implementation of log matching property
- Efficient log compaction mechanism

### Sharding
- Complex state transfer during reconfigurations
- Maintaining consistency during migrations
- Handling network partitions

### Performance
- Balancing between consistency and performance
- Optimizing for different workload patterns
- Managing resource utilization

## Testing Strategy

1. **Unit Tests**
   - Comprehensive coverage of core logic
   - Extensive state machine testing
   - Property-based testing for protocol invariants

2. **Integration Tests**
   - Full system tests with multiple nodes
   - Network partition scenarios
   - Performance regression tests

3. **Chaos Testing**
   - Random fault injection
   - Network latency simulation
   - Load pattern variations

## Future Considerations

1. **Scalability Improvements**
   - Read-only replicas
   - Geographic distribution
   - Multi-datacenter support

2. **Feature Extensions**
   - Transactional support
   - Advanced querying capabilities
   - Pluggable storage engines

3. **Operational Improvements**
   - Enhanced monitoring
   - Automated operations
   - Self-healing capabilities
