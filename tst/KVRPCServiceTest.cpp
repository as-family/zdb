
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
#include <proto/kvStore.pb.h>

using namespace zdb;

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
    service.connect();
    EXPECT_TRUE(service.available());
    testServer->shutdown(); // Simulate server failure
    kvStore::GetRequest req;
    req.set_key("key");
    kvStore::GetReply rep;
    service.call(&kvStore::KVStoreService::Stub::get, req, rep);
    EXPECT_FALSE(service.available());
}


TEST_F(KVRPCServiceTest, CallGetSuccess) {
    KVRPCService service{address, policy};
    service.connect();
    kvStore::SetRequest setReq;
    setReq.set_key("foo");
    setReq.set_value("bar");
    kvStore::SetReply setRep;
    service.call(&kvStore::KVStoreService::Stub::set, setReq, setRep);
    kvStore::GetRequest req;
    req.set_key("foo");
    kvStore::GetReply rep;
    auto result = service.call(&kvStore::KVStoreService::Stub::get, req, rep);
    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(rep.value(), "bar");
}


TEST_F(KVRPCServiceTest, CallSetSuccess) {
    KVRPCService service{address, policy};
    service.connect();
    kvStore::SetRequest req;
    req.set_key("foo");
    req.set_value("bar");
    kvStore::SetReply rep;
    auto result = service.call(&kvStore::KVStoreService::Stub::set, req, rep);
    EXPECT_TRUE(result.has_value());
}


TEST_F(KVRPCServiceTest, CallEraseSuccess) {
    KVRPCService service{address, policy};
    service.connect();
    kvStore::SetRequest setReq;
    setReq.set_key("foo");
    setReq.set_value("bar");
    kvStore::SetReply setRep;
    service.call(&kvStore::KVStoreService::Stub::set, setReq, setRep);
    kvStore::EraseRequest req;
    req.set_key("foo");
    kvStore::EraseReply rep;
    auto result = service.call(&kvStore::KVStoreService::Stub::erase, req, rep);
    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(rep.value(), "bar");
}


TEST_F(KVRPCServiceTest, CallSizeSuccess) {
    KVRPCService service{address, policy};
    service.connect();
    kvStore::SetRequest setReq;
    setReq.set_key("foo");
    setReq.set_value("bar");
    kvStore::SetReply setRep;
    service.call(&kvStore::KVStoreService::Stub::set, setReq, setRep);
    const kvStore::SizeRequest req;
    kvStore::SizeReply rep;
    auto result = service.call(&kvStore::KVStoreService::Stub::size, req, rep);
    EXPECT_TRUE(result.has_value());
    EXPECT_GE(rep.size(), 1);
}


TEST_F(KVRPCServiceTest, CallFailureReturnsError) {
    KVRPCService service{address, policy};
    service.connect();
    kvStore::GetRequest req;
    req.set_key("notfound");
    kvStore::GetReply rep;
    auto result = service.call(&kvStore::KVStoreService::Stub::get, req, rep);
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, ErrorCode::NotFound);
}
