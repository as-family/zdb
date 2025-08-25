
#include "common/RetryPolicy.hpp"
#include "common/Error.hpp"
#include "server/KVStoreServer.hpp"
#include "server/InMemoryKVStore.hpp"
#include <gtest/gtest.h>
#include <grpcpp/grpcpp.h>
#include <thread>
#include <chrono>
#include <string>
#include <utility>
#include <grpcpp/security/credentials.h>
#include <memory>
#include <proto/kvStore.pb.h>
#include <proto/kvStore.grpc.pb.h>
#include <proto/types.pb.h>
#include "common/Types.hpp"
#include "client/Config.hpp"

using zdb::Value;
using zdb::InMemoryKVStore;
using zdb::KVStoreServiceImpl;
using zdb::RetryPolicy;
using zdb::ErrorCode;
using zdb::kvStore::GetRequest;
using zdb::kvStore::GetReply;
using zdb::kvStore::SetRequest;
using zdb::kvStore::SetReply;
using zdb::kvStore::EraseRequest;
using zdb::kvStore::EraseReply;
using zdb::kvStore::SizeRequest;
using zdb::kvStore::SizeReply;


// Helper to start a real gRPC server for integration tests
class TestKVServer {
public:
    explicit TestKVServer(std::string addr) : kvStore{}, serviceImpl{kvStore, nullptr, nullptr}, server{nullptr}, address{std::move(addr)} {
        grpc::ServerBuilder builder;
        builder.AddListeningPort(address, grpc::InsecureServerCredentials());
        builder.RegisterService(&serviceImpl);
        server = builder.BuildAndStart();
    }
    void shutdown() {
        if (server) {
            server->Shutdown();
        }
    }
private:
    InMemoryKVStore kvStore;
    KVStoreServiceImpl serviceImpl;
    std::unique_ptr<grpc::Server> server;
    std::string address;
};

class KVRPCServiceTest : public ::testing::Test {
protected:
    RetryPolicy policy{std::chrono::microseconds(100), std::chrono::microseconds(1000), std::chrono::microseconds(5000), 2, 0};
    std::string address{"localhost:50051"};
    std::unique_ptr<TestKVServer> testServer;
    void SetUp() override {
        testServer = std::make_unique<TestKVServer>(address);
        // Give server time to start
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }
    void TearDown() override {
        testServer->shutdown();
        testServer.reset();
    }
};


TEST_F(KVRPCServiceTest, ConnectSuccess) {
    zdb::KVRPCService service{address, policy};
    auto result = service.connect();
    EXPECT_TRUE(result.has_value());
    EXPECT_TRUE(service.available());
}


TEST_F(KVRPCServiceTest, ConnectFailure) {
    zdb::KVRPCService badService{"localhost:59999", policy}; // unlikely port
    auto result = badService.connect();
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, ErrorCode::Unknown);
}


TEST_F(KVRPCServiceTest, availableReflectsCircuitBreaker) {
    zdb::KVRPCService service{address, policy};
    EXPECT_TRUE(service.connect().has_value());
    EXPECT_TRUE(service.available());
    testServer->shutdown(); // Simulate server failure
    GetRequest req;
    req.mutable_key()->set_data("key");
    GetReply rep;
    EXPECT_FALSE(service.call("get", &zdb::kvStore::KVStoreService::Stub::get, req, rep).has_value());
    EXPECT_FALSE(service.available());
}


TEST_F(KVRPCServiceTest, CallGetSuccess) {
    zdb::KVRPCService service{address, policy};
    EXPECT_TRUE(service.connect().has_value());
    SetRequest setReq;
    setReq.mutable_key()->set_data("foo");
    setReq.mutable_value()->set_data("bar");
    SetReply setRep;
    EXPECT_TRUE(
        service.call("set", &zdb::kvStore::KVStoreService::Stub::set, setReq, setRep).has_value());
    GetRequest req;
    req.mutable_key()->set_data("foo");
    GetReply rep;
    auto result = service.call("get", &zdb::kvStore::KVStoreService::Stub::get, req, rep);
    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(rep.value().data(), "bar");
}


TEST_F(KVRPCServiceTest, CallSetSuccess) {
    zdb::KVRPCService service{address, policy};
    EXPECT_TRUE(service.connect().has_value());
    SetRequest req;
    req.mutable_key()->set_data("foo");
    req.mutable_value()->set_data("bar");
    SetReply rep;
    auto result = service.call("set", &zdb::kvStore::KVStoreService::Stub::set, req, rep);
    EXPECT_TRUE(result.has_value());
}


TEST_F(KVRPCServiceTest, CallEraseSuccess) {
    zdb::KVRPCService service{address, policy};
    EXPECT_TRUE(service.connect().has_value());
    SetRequest setReq;
    setReq.mutable_key()->set_data("foo");
    setReq.mutable_value()->set_data("bar");
    SetReply setRep;
    EXPECT_TRUE(service.call("set", &zdb::kvStore::KVStoreService::Stub::set, setReq, setRep).has_value());
    EraseRequest req;
    req.mutable_key()->set_data("foo");
    EraseReply rep;
    auto result = service.call("erase", &zdb::kvStore::KVStoreService::Stub::erase, req, rep);
    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(rep.value().data(), "bar");
}


TEST_F(KVRPCServiceTest, CallSizeSuccess) {
    zdb::KVRPCService service{address, policy};
    EXPECT_TRUE(service.connect().has_value());
    SetRequest setReq;
    setReq.mutable_key()->set_data("foo");
    setReq.mutable_value()->set_data("bar");
    SetReply setRep;
    EXPECT_TRUE(service.call("set", &zdb::kvStore::KVStoreService::Stub::set, setReq, setRep).has_value());
    const SizeRequest req;
    SizeReply rep;
    auto result = service.call("size", &zdb::kvStore::KVStoreService::Stub::size, req, rep);
    EXPECT_TRUE(result.has_value());
    EXPECT_GE(rep.size(), 1);
}


TEST_F(KVRPCServiceTest, CallFailureReturnsError) {
    zdb::KVRPCService service{address, policy};
    EXPECT_TRUE(service.connect().has_value());
    GetRequest req;
    req.mutable_key()->set_data("notfound");
    GetReply rep;
    auto result = service.call("get", &zdb::kvStore::KVStoreService::Stub::get, req, rep);
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error().back().code, ErrorCode::KeyNotFound);
}

// Test connection reuse when channel is already READY
TEST_F(KVRPCServiceTest, ConnectReuseReadyChannel) {
    zdb::KVRPCService service{address, policy};
    
    // First connection
    auto result1 = service.connect();
    EXPECT_TRUE(result1.has_value());
    EXPECT_TRUE(service.connected());
    
    // Second connect call should reuse existing channel
    auto result2 = service.connect();
    EXPECT_TRUE(result2.has_value());
    EXPECT_TRUE(service.connected());
}

// Test that available() can trigger reconnection
TEST_F(KVRPCServiceTest, AvailableTriggersReconnection) {
    zdb::KVRPCService service{address, policy};
    
    // Initially not connected
    EXPECT_FALSE(service.connected());
    
    // available() should attempt connection
    const bool available = service.available();
    EXPECT_TRUE(available);
    EXPECT_TRUE(service.connected());
}

// Test available() returns false when circuit breaker is open
TEST_F(KVRPCServiceTest, AvailableReturnsFalseWhenCircuitBreakerOpen) {
    // Use a policy that opens circuit breaker quickly
    const RetryPolicy quickFailPolicy{std::chrono::microseconds(10), std::chrono::microseconds(50), std::chrono::microseconds(100), 1, 1};
    zdb::KVRPCService service{address, quickFailPolicy};
    
    EXPECT_TRUE(service.connect().has_value());
    EXPECT_TRUE(service.available());
    
    // Shutdown server to trigger circuit breaker
    testServer->shutdown();
    
    // Make a call that will fail and open circuit breaker
    GetRequest req;
    req.mutable_key()->set_data("test");
    GetReply rep;
    auto result = service.call("get", &zdb::kvStore::KVStoreService::Stub::get, req, rep);
    EXPECT_FALSE(result.has_value());
    
    // available() should now return false due to circuit breaker
    EXPECT_FALSE(service.available());
}

// Test connected() reflects actual gRPC channel state
TEST_F(KVRPCServiceTest, ConnectedReflectsChannelState) {
    zdb::KVRPCService service{address, policy};
    
    // Initially not connected
    EXPECT_FALSE(service.connected());
    
    // After successful connection
    EXPECT_TRUE(service.connect().has_value());
    EXPECT_TRUE(service.connected());
    
    // After server shutdown, should eventually show as not connected
    testServer->shutdown();
    
    // Give some time for gRPC to detect the disconnection
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    // Note: gRPC channel state changes are asynchronous, so we might need to trigger activity
    GetRequest req;
    req.mutable_key()->set_data("test");
    GetReply rep;
    EXPECT_FALSE(service.call("get", &zdb::kvStore::KVStoreService::Stub::get, req, rep).has_value()); // This will fail and potentially update channel state
}

// Test connection reuse with IDLE channel state
TEST_F(KVRPCServiceTest, ConnectHandlesIdleChannel) {
    zdb::KVRPCService service{address, policy};
    
    // First connection
    EXPECT_TRUE(service.connect().has_value());
    EXPECT_TRUE(service.connected());
    
    // Wait for channel to potentially go idle (this is implementation-dependent)
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    
    // Second connect should handle idle state gracefully
    auto result = service.connect();
    EXPECT_TRUE(result.has_value());
}

// Test multiple consecutive calls to available()
TEST_F(KVRPCServiceTest, MultipleAvailableCalls) {
    zdb::KVRPCService service{address, policy};
    
    // Multiple calls should be consistent
    for (int i = 0; i < 5; ++i) {
        EXPECT_TRUE(service.available());
        EXPECT_TRUE(service.connected());
    }
}

// Test available() behavior after connection failure
TEST_F(KVRPCServiceTest, AvailableAfterConnectionFailure) {
    // Connect to invalid address
    zdb::KVRPCService badService{"localhost:99999", policy};
    
    // available() should return false when connection fails
    EXPECT_FALSE(badService.available());
    EXPECT_FALSE(badService.connected());
}

// Test reconnection after server restart
TEST_F(KVRPCServiceTest, ReconnectionAfterServerRestart) {
    zdb::KVRPCService service{address, policy};
    
    // Initial connection
    EXPECT_TRUE(service.connect().has_value());
    EXPECT_TRUE(service.connected());
    
    // Shutdown server temporarily
    testServer->shutdown();
    
    // Give time for disconnection
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    // Create new server instance (simulating restart)
    auto newServer = std::make_unique<TestKVServer>(address);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    // available() should trigger reconnection
    EXPECT_TRUE(service.available());
    EXPECT_TRUE(service.connected());
    
    // Clean up the new server
    newServer->shutdown();
}

// Test that connect() creates stub when missing
TEST_F(KVRPCServiceTest, ConnectCreatesStubWhenMissing) {
    zdb::KVRPCService service{address, policy};
    
    // Connect should succeed and service should be ready for calls
    EXPECT_TRUE(service.connect().has_value());
    EXPECT_TRUE(service.connected());
    
    // Should be able to make successful calls (indicating stub was created)
    SetRequest req;
    req.mutable_key()->set_data("test");
    req.mutable_value()->set_data("value");
    SetReply rep;
    auto result = service.call("set", &zdb::kvStore::KVStoreService::Stub::set, req, rep);
    EXPECT_TRUE(result.has_value());
}

// Test circuit breaker integration with available()
TEST_F(KVRPCServiceTest, CircuitBreakerIntegrationWithAvailable) {
    const RetryPolicy circuitBreakerPolicy{std::chrono::milliseconds(10), std::chrono::milliseconds(50), std::chrono::milliseconds(200), 1, 1};
    zdb::KVRPCService service{address, circuitBreakerPolicy};
    
    EXPECT_TRUE(service.connect().has_value());
    EXPECT_TRUE(service.available());
    
    // Shutdown server and trigger circuit breaker
    testServer->shutdown();
    
    // Make calls that will fail and open circuit breaker
    GetRequest req;
    req.mutable_key()->set_data("test");
    GetReply rep;
    EXPECT_FALSE(service.call("get", &zdb::kvStore::KVStoreService::Stub::get, req, rep).has_value());

    // available() should return false when circuit breaker is open
    EXPECT_FALSE(service.available());
    
    // Restart server
    testServer = std::make_unique<TestKVServer>(address);
    std::this_thread::sleep_for(std::chrono::seconds(2)); // Wait for circuit breaker reset
    
    // available() should trigger reconnection after circuit breaker reset
    EXPECT_TRUE(service.available());
    EXPECT_TRUE(service.connected());
}

// Test address() method consistency
TEST_F(KVRPCServiceTest, AddressMethodConsistency) {
    const std::string testAddr = "test.example.com:1234";
    zdb::KVRPCService service{testAddr, policy};
    
    EXPECT_EQ(service.address(), testAddr);
    
    // Address should remain consistent regardless of connection state
    auto result = service.connect(); // This will fail for invalid address
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(service.address(), testAddr);
}

// Test non-const available() can modify state
TEST_F(KVRPCServiceTest, AvailableCanModifyState) {
    zdb::KVRPCService service{address, policy};
    
    // Initially not connected
    EXPECT_FALSE(service.connected());
    
    // available() is non-const and can trigger connection
    EXPECT_TRUE(service.available());
    
    // State should now be modified (connected)
    EXPECT_TRUE(service.connected());
}

// Test behavior with rapid server cycling
TEST_F(KVRPCServiceTest, RapidServerCycling) {
    zdb::KVRPCService service{address, policy};
    
    // Initial connection
    EXPECT_TRUE(service.connect().has_value());
    EXPECT_TRUE(service.available());
    
    // Create temporary servers for cycling test
    std::vector<std::unique_ptr<TestKVServer>> tempServers;
    
    // Cycle server multiple times
    for (int i = 0; i < 3; ++i) {
        testServer->shutdown();
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        
        tempServers.push_back(std::make_unique<TestKVServer>(address));
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        
        // available() should handle reconnection
        EXPECT_TRUE(service.available());
        
        tempServers.back()->shutdown();
    }
    
    // Clean up temp servers
    tempServers.clear();
}

// Test error handling in available() when reconnection fails
TEST_F(KVRPCServiceTest, AvailableHandlesReconnectionFailure) {
    zdb::KVRPCService service{address, policy};
    
    // Initial connection
    EXPECT_TRUE(service.connect().has_value());
    EXPECT_TRUE(service.available());
    
    // Shutdown server but don't reset - let TearDown handle cleanup
    testServer->shutdown();
    
    // Give time for disconnection
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    // available() should return false when reconnection fails
    EXPECT_FALSE(service.available());
    EXPECT_FALSE(service.connected());
}
