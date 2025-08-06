
#include "client/KVRPCService.hpp"
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

using zdb::InMemoryKVStore;
using zdb::KVStoreServiceImpl;
using zdb::KVStoreServer;
using zdb::KVRPCService;
using zdb::RetryPolicy;
using zdb::Error;
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
    explicit TestKVServer(std::string addr) : kvStore{}, serviceImpl{kvStore}, server{nullptr}, address{std::move(addr)} {
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
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    void TearDown() override {
        testServer->shutdown();
        testServer.reset();
    }
};


TEST_F(KVRPCServiceTest, ConnectSuccess) {
    KVRPCService service{address, policy};
    auto result = service.connect();
    EXPECT_TRUE(result.has_value());
    EXPECT_TRUE(service.available());
}


TEST_F(KVRPCServiceTest, ConnectFailure) {
    KVRPCService badService{"localhost:59999", policy}; // unlikely port
    auto result = badService.connect();
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, ErrorCode::Unknown);
}


TEST_F(KVRPCServiceTest, availableReflectsCircuitBreaker) {
    KVRPCService service{address, policy};
    EXPECT_TRUE(service.connect().has_value());
    EXPECT_TRUE(service.available());
    testServer->shutdown(); // Simulate server failure
    GetRequest req;
    req.set_key("key");
    GetReply rep;
    EXPECT_TRUE(!service.call(&zdb::kvStore::KVStoreService::Stub::get, req, rep).has_value());
    EXPECT_FALSE(service.available());
}


TEST_F(KVRPCServiceTest, CallGetSuccess) {
    KVRPCService service{address, policy};
    EXPECT_TRUE(service.connect().has_value());
    SetRequest setReq;
    setReq.set_key("foo");
    setReq.set_value("bar");
    SetReply setRep;
    EXPECT_TRUE(
        service.call(&zdb::kvStore::KVStoreService::Stub::set, setReq, setRep).has_value());
    GetRequest req;
    req.set_key("foo");
    GetReply rep;
    auto result = service.call(&zdb::kvStore::KVStoreService::Stub::get, req, rep);
    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(rep.value(), "bar");
}


TEST_F(KVRPCServiceTest, CallSetSuccess) {
    KVRPCService service{address, policy};
    EXPECT_TRUE(service.connect().has_value());
    SetRequest req;
    req.set_key("foo");
    req.set_value("bar");
    SetReply rep;
    auto result = service.call(&zdb::kvStore::KVStoreService::Stub::set, req, rep);
    EXPECT_TRUE(result.has_value());
}


TEST_F(KVRPCServiceTest, CallEraseSuccess) {
    KVRPCService service{address, policy};
    EXPECT_TRUE(service.connect().has_value());
    SetRequest setReq;
    setReq.set_key("foo");
    setReq.set_value("bar");
    SetReply setRep;
    EXPECT_TRUE(service.call(&zdb::kvStore::KVStoreService::Stub::set, setReq, setRep).has_value());
    EraseRequest req;
    req.set_key("foo");
    EraseReply rep;
    auto result = service.call(&zdb::kvStore::KVStoreService::Stub::erase, req, rep);
    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(rep.value(), "bar");
}


TEST_F(KVRPCServiceTest, CallSizeSuccess) {
    KVRPCService service{address, policy};
    EXPECT_TRUE(service.connect().has_value());
    SetRequest setReq;
    setReq.set_key("foo");
    setReq.set_value("bar");
    SetReply setRep;
    EXPECT_TRUE(service.call(&zdb::kvStore::KVStoreService::Stub::set, setReq, setRep).has_value());
    const SizeRequest req;
    SizeReply rep;
    auto result = service.call(&zdb::kvStore::KVStoreService::Stub::size, req, rep);
    EXPECT_TRUE(result.has_value());
    EXPECT_GE(rep.size(), 1);
}


TEST_F(KVRPCServiceTest, CallFailureReturnsError) {
    KVRPCService service{address, policy};
    EXPECT_TRUE(service.connect().has_value());
    GetRequest req;
    req.set_key("notfound");
    GetReply rep;
    auto result = service.call(&zdb::kvStore::KVStoreService::Stub::get, req, rep);
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, ErrorCode::NotFound);
}
