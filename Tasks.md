This is an excellent idea for accountability and managing such an ambitious plan! We'll integrate all the revisions and specific elaborations into this comprehensive checklist.

**Key:**
*   [ ] = Not started
*   [x] = Completed
*   [~] = In progress / Partially completed
*   [<] = Deferred to a later phase (with rationale)

---

### **Comprehensive Study Plan Checklist (Phase 0 - Phase 1, Week 3)**

**Overall Plan Goals:**
*   Apply for senior engineering role in 6 months.
*   Deepen Distributed Systems & Parallel Programming.
*   Create an open-source distributed database project.

---

#### **Phase 0: Setup & Mental Reset (Completed before Week 1)**

*   **Project Definition & Setup:**
    *   [x] Distributed DB Project: Brainstormed core features (KV store, Raft, replicated, strongly consistent).
    *   [x] C++23 CMake Project Skeleton created and verified.
    *   [x] gRPC peer communication verified (can send/receive basic messages).
    *   [x] Initial Brainstorming/Architecture Sketch completed.
    *   [x] Environment Setup (C++ dev tools, build system, etc.) verified.
    *   [x] Self-Care & Study Schedule planned and committed to.

---

#### **Phase 1: Core Foundations & C++ Mastery (Weeks 1-4)**

**Week 1: C++ Concurrency Deep Dive & Basic In-Memory KV**

*   **Study Focus:**
    *   [x] C++ Concurrency: `std::thread`, `std::mutex`, `std::unique_lock`, `std::condition_variable` (conceptual review).
    *   [x] C++ Concurrency: `std::atomic` and memory orders (conceptual review).
    *   [x] OS/Networking Review: OS thread management, mutual exclusion (OSTEP/6.1800 relevant sections).
    *   [x] OS/Networking Review: IPC basics (sockets, pipes, etc., conceptual).
*   **Project Work:**
    *   [x] **InMemoryKVStore Creation:** `InMemoryKVStore` class with `get/set/remove/size` using `std::unordered_map`.
    *   [x] **Thread-Safety (Coarse-Grained):** `InMemoryKVStore` made thread-safe using `std::mutex` or `std::shared_mutex` on the entire map.
    *   [x] **Unit Tests for `InMemoryKVStore` (Basic):** Tests confirming thread-safety and correctness for basic operations.
    *   [x] **gRPC Service Definition:** `KvService.proto` defined with `Get/Set/Delete` messages.
    *   [x] **gRPC Service Implementation:** `KvServiceImpl` implemented, using `InMemoryKVStore`.
    *   [x] **Basic gRPC Client:** Client to interact with single-node server for `set/get/delete`.
    *   [x] **TSan Usage:** Ran tests with `ThreadSanitizer` to detect race conditions (initial pass).

---

**Week 2 (Adjusted Progress - Starting Point for Week 3)**

*   **Study Focus:**
    *   [x] C++ Advanced: Smart Pointers (`std::unique_ptr`, `std::shared_ptr`, `std::weak_ptr`) & RAII principles. (Initial reads done)
    *   [x] C++ Advanced: Error Handling (`std::expected` design & principles).
    *   [x] OS/Networking Review: I/O Multiplexing (concepts of `epoll`/`kqueue`), why they're important for high-perf servers.
*   **Project Work:**
    *   [x] **Smart Pointers Refactoring (Ownership):** Replaced raw pointers with `std::unique_ptr` where appropriate (`main`, `KvClient`).
    *   [x] **Smart Pointers Refactoring (Shared Ownership):** Applied `std::shared_ptr` (e.g., `KvServiceImpl` holding `InMemoryKVStore`).
    *   [x] **Error Handling (`db_errors.h`):** Defined `MyDB::ErrorCode` (starting from 1) and `MyDB::Error` struct.
    *   [x] **Error Handling (`grpc_error_converter.h`):** Implemented `ToGrpcStatus` and `FromGrpcStatus` helpers for clean Protobuf `ErrorDetail` integration.
    *   [x] **`InMemoryKVStore` Error Handling:** Methods now return `std::expected<T, Error>` for recoverable errors (e.g., `InvalidArgument`).
    *   [x] **Server-Side gRPC Error Integration:** `KvServiceImpl` methods use `GrpcError::WrapExpectedToGrpcStatus` or `GrpcError::ToGrpcStatus` for clean error propagation.
    *   [x] **Client-Side gRPC Error Integration:** `KvClient` methods use `GrpcError::FromGrpcStatus` to handle RPC statuses as `std::expected<void, Error>`.
    *   [x] **Client Robustness (Basic Retries):** Basic retry loop implemented in client.
    *   [x] **Client Robustness (Multi-Address):** Client manages a list of server addresses (e.g., `ClusterClientConfig`).
    *   [x] **Unit/Integration Tests for Errors & Client Retries:** (In progress / Needs more completion)
    *   [x] **Logging Integration (`spdlog`):** `spdlog` initialized, `spdlog` calls replacing `std::cout`/`printf` (partial, needs to be everywhere).
    *   [ ] **Basic Metrics System:** `MetricsManager`, `Counter`, `Gauge` classes implemented. Code instrumented with `GET_COUNTER`/`GET_GAUGE` (partial).
    *   [ ] **Metrics Exposure:** (Deferred to later in Week 3 / Phase 2 - will start with simple print in `main`).
    *   [ ] **Day 6 Tasks (Refinement, Review, Planning):** (Needs to be done, will integrate this week).

---

**Week 3: CI, Static Analysis, Refactoring, and Tests (Current Week)**

*   **Day 1: GitHub Actions CI Foundation**
    *   [x] Add single-platform CI workflow: checkout, cache vcpkg, configure/build with CMake presets.
    *   [x] Run tests via ctest and upload build/install artifacts.
    *   [x] Add release job on master pushes to package artifacts (split staging/archiving).
    *   [ ] Add CI badges to `README.md` (build status, artifacts/reports note).

*   **Day 2: Static Analysis Integration**
    *   [x] Add `.clang-tidy` with modernize, cert, bugprone, concurrency, readability checks and tuned suppressions.
    *   [x] Create `run-clang-tidy.sh` to produce HTML/YAML reports; timestamped outputs under `clang-tidy-reports/`.
    *   [x] Add `cppcheck` step producing XML; upload both analyses as CI artifacts.
    *   [x] Document static analysis usage in `README.md` (script + optional CMake toggles).
    *   [x] Make CI fail on critical categories (`WarningsAsErrors` set) and wire gating in workflow.
    *   [ ] Add optional local build toggle `-DENABLE_CLANG_TIDY=ON` verification in CMake (ensure target wiring).

*   **Day 3: Reliability Refactors**
    *   [x] Introduce `RetryPolicy`, `ExponentialBackoff`, `FullJitter`, `Repeater` utilities in `common/`.
    *   [x] Add `CircuitBreaker` around gRPC stubs in `KVRPCService`.
    *   [x] Refactor client (`Config`, `KVStoreClient`, `KVRPCService`) for multi-peer retry and backoff.
    *   [x] Expand structured logging (`spdlog`) across client/common/server paths.
    *   [x] Sweep for naming/style to satisfy `.clang-tidy` identifier rules across modules.

*   **Day 4: Test Expansion & Fixes**
    *   [x] Add/expand tests: `CircuitBreakerTest`, `ConfigTest`, `KVRPCServiceTest`, `KVStoreClientTest`, `KVStoreServerTest`, `ErrorConverterTest`, `ExponentialBackoffTest`, `FullJitterTest`, `InMemoryKVStoreTest`, `RepeaterTest`, `RetryPolicyTest`.
    *   [x] Create client retry integration tests and fix resulting issues.
    *   [x] Add negative/failure-path tests (timeouts, circuit open/half-open, unreachable peers).
    *   [ ] Add ThreadSanitizer run (preset) and optional sanitizer CI job.

*   **Day 5: Structure & Proto Hygiene**
    *   [x] Restructure code into `client/`, `server/`, `common/`, `zdb/` directories; update CMake targets.
    *   [x] Organize `proto/` and ensure generated sources are linked into tests/apps via CMake.
    *   [ ] Add proto lint/format step (optional) and ensure deterministic codegen in CI.

*   **Day 6: Polish & Docs**
    *   [x] Update `README.md` with build/test/static analysis instructions and usage examples.
    *   [ ] Add CI status badge and link to analysis artifacts section.
    *   [ ] Address top-priority clang-tidy findings (or add targeted suppressions where justified).
    *   [ ] Open follow-up issues: sanitizer jobs, code coverage, multi-platform matrix (Linux/Clang/GCC; macOS; Windows optional).

---

**Week 4: The Refactoring Capstone**
*   **Day 1 (Monday, Aug 11): Define New Types & Abstract Interfaces**

**Goal:** Lay the new foundation without breaking existing logic.
*   [ ] **Project:** Create `src/db_types.h`. Define the concrete `struct MyDB::Key` (with `key_data` and `KeyHash`) and `struct MyDB::Value` (with `value_data` and `version`).
*   [ ] **Project:** Update your `.proto` files. Create `KeyProto` and `ValueProto` messages and update `GetRequest`, `GetResponse`, and `SetRequest` to use them.
*   [ ] **Project:** Run `protoc` to generate the new C++ Protobuf code.
*   [ ] **Project:** Create `storage_engine.h`. Define the abstract `class StorageEngine` with pure virtual functions (`Set`, `Get`, `Delete`) that use `const Key&` and `const Value&`.
*   [ ] **Git:** Create a new feature branch (e.g., `feature/storage-refactor`). Commit these changes. The project should still compile. Push to trigger CI.

*   **Day 2 (Tuesday, Aug 12): Refactor Concrete Storage & Service Layer**

**Goal:** Make the server-side conform to the new design.
*   [ ] **Project:** Refactor `InMemoryKVStore` into `InMemoryStorageEngine`, making it inherit from `StorageEngine`.
*   [ ] **Project:** Update its internal `std::unordered_map` to use `Key` and `Value` (`std::unordered_map<Key, Value, KeyHash>`).
*   [ ] **Project:** Update the unit tests for `InMemoryStorageEngine` to use and verify the new `Key` and `Value` structs. Ensure they pass with TSan.
*   [ ] **Project:** Refactor `KvServiceImpl` to handle the new Protobuf messages, translating them to/from the internal `Key`/`Value` structs when calling the `StorageEngine` interface.
*   [ ] **Git:** Commit the server-side refactoring. Verify CI passes.


*   **Day 3 (Wednesday, Aug 13): Refactor Application & Client Layers**

**Goal:** Complete the refactoring end-to-end.
*   [ ] **Project:** Refactor your `RaftNode` class (or `main.cpp` if `RaftNode` isn't fully fleshed out) to use **Dependency Injection**. It should hold a `std::unique_ptr<StorageEngine>` and receive it in its constructor.
*   [ ] **Project:** Refactor the `KvClient` to use the `Key` and `Value` structs in its public API, handling the translation to/from Protobuf internally.
*   [ ] **Project:** Update all integration and end-to-end tests to use the new client API.
*   [ ] **Git:** Commit the client-side and application-layer refactoring.

---

*   **Day 4 (Thursday, Aug 14): Final Verification & Phase 2 Preparation**

**Goal:** Lock in the Phase 1 achievement and prepare mentally for Raft.
*   [ ] **Testing:**
    *   [ ] Run the **entire** test suite from a clean build.
    *   [ ] Run the entire suite again with **TSan enabled**. Fix any final race conditions or memory issues.
*   [ ] **Git & CI/CD:**
    *   [ ] Push all final changes to your feature branch.
    *   [ ] Create a Pull Request. Verify your GitHub Actions pipeline passes all checks.
    *   [ ] Review the PR (self-review or with Copilot) and **merge it into `master`**.
*   [ ] **Documentation:**
    *   [ ] Update your `README.md`. Add a section on "Architecture" that describes your use of abstract interfaces (`StorageEngine`) and concrete data types (`Key`, `Value`). This documents your achievement.
*   [ ] **Study (Phase 2 Kickoff):**
    *   [ ] **Action:** Begin your Raft study. Watch **LEC 3 (Primary-Backup)** and **LEC 4 (Consistency)** from the 6.824 schedule.
    *   [ ] **Goal:** Your goal for today's study is to understand the *context* and *goals* of a system like Raft. You will dive into Raft itself on Saturday.

---
