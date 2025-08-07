
#include <gtest/gtest.h>
#include <grpcpp/grpcpp.h>
#include "server/KVStoreServer.hpp"
#include "server/InMemoryKVStore.hpp"
#include "proto/kvStore.grpc.pb.h"
#include <thread>
#include <chrono>
#include <string>
#include <memory>
#include "proto/kvStore.pb.h"
#include <grpcpp/support/status.h>
#include <grpcpp/security/credentials.h>

using zdb::InMemoryKVStore;
using zdb::KVStoreServiceImpl;
using zdb::KVStoreServer;
using zdb::kvStore::GetRequest;
using zdb::kvStore::GetReply;
using zdb::kvStore::SetRequest;
using zdb::kvStore::SetReply;
using zdb::kvStore::EraseRequest;
using zdb::kvStore::EraseReply;
using zdb::kvStore::SizeRequest;
using zdb::kvStore::SizeReply;

const std::string SERVER_ADDR = "localhost:50051";

class KVStoreServerTest : public ::testing::Test {
protected:
    InMemoryKVStore kvStore;
    KVStoreServiceImpl serviceImpl {kvStore};
    std::unique_ptr<KVStoreServer> server;
    std::thread serverThread;

    void SetUp() override {
        server = std::make_unique<KVStoreServer>(SERVER_ADDR, serviceImpl);
        serverThread = std::thread([this]() { server->wait(); });
        // Wait for server to start
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }

    void TearDown() override {
        // Gracefully shutdown the server
        if (server) {
            server->shutdown();
        }
        if (serverThread.joinable()) {
            serverThread.join();
        }
    }
};

TEST_F(KVStoreServerTest, SetAndGetSuccess) {
    auto channel = grpc::CreateChannel(SERVER_ADDR, grpc::InsecureChannelCredentials());
    std::unique_ptr<zdb::kvStore::KVStoreService::Stub> stub = zdb::kvStore::KVStoreService::NewStub(channel);

    SetRequest setReq;
    setReq.set_key("foo");
    setReq.set_value("bar");
    SetReply setRep;
    grpc::ClientContext ctx1;
    auto status = stub->set(&ctx1, setReq, &setRep);
    ASSERT_TRUE(status.ok());

    GetRequest getReq;
    getReq.set_key("foo");
    GetReply getRep;
    auto ctx2 = grpc::ClientContext();
    status = stub->get(&ctx2, getReq, &getRep);
    ASSERT_TRUE(status.ok());
    ASSERT_EQ(getRep.value(), "bar");
}

TEST_F(KVStoreServerTest, GetNotFound) {
    auto channel = grpc::CreateChannel(SERVER_ADDR, grpc::InsecureChannelCredentials());
    std::unique_ptr<zdb::kvStore::KVStoreService::Stub> stub = zdb::kvStore::KVStoreService::NewStub(channel);

    GetRequest getReq;
    getReq.set_key("missing");
    GetReply getRep;
    grpc::ClientContext ctx;
    auto status = stub->get(&ctx, getReq, &getRep);
    ASSERT_FALSE(status.ok());
    ASSERT_EQ(status.error_code(), grpc::StatusCode::NOT_FOUND);
}

TEST_F(KVStoreServerTest, SetOverwrite) {
    auto channel = grpc::CreateChannel(SERVER_ADDR, grpc::InsecureChannelCredentials());
    std::unique_ptr<zdb::kvStore::KVStoreService::Stub> stub = zdb::kvStore::KVStoreService::NewStub(channel);

    SetRequest setReq;
    setReq.set_key("foo");
    setReq.set_value("bar");
    SetReply setRep;
    grpc::ClientContext ctx1;
    auto status = stub->set(&ctx1, setReq, &setRep);
    ASSERT_TRUE(status.ok());

    setReq.set_value("baz");
    grpc::ClientContext ctx2;
    status = stub->set(&ctx2, setReq, &setRep);
    ASSERT_TRUE(status.ok());

    GetRequest getReq;
    getReq.set_key("foo");
    GetReply getRep;
    grpc::ClientContext ctx3;
    status = stub->get(&ctx3, getReq, &getRep);
    ASSERT_TRUE(status.ok());
    ASSERT_EQ(getRep.value(), "baz");
}

TEST_F(KVStoreServerTest, EraseSuccess) {
    auto channel = grpc::CreateChannel(SERVER_ADDR, grpc::InsecureChannelCredentials());
    std::unique_ptr<zdb::kvStore::KVStoreService::Stub> stub = zdb::kvStore::KVStoreService::NewStub(channel);

    SetRequest setReq;
    setReq.set_key("foo");
    setReq.set_value("bar");
    SetReply setRep;
    grpc::ClientContext ctx1;
    auto status = stub->set(&ctx1, setReq, &setRep);
    ASSERT_TRUE(status.ok());

    EraseRequest eraseReq;
    eraseReq.set_key("foo");
    EraseReply eraseRep;
    grpc::ClientContext ctx2;
    status = stub->erase(&ctx2, eraseReq, &eraseRep);
    ASSERT_TRUE(status.ok());
    ASSERT_EQ(eraseRep.value(), "bar");

    GetRequest getReq;
    getReq.set_key("foo");
    GetReply getRep;
    grpc::ClientContext ctx3;
    status = stub->get(&ctx3, getReq, &getRep);
    ASSERT_FALSE(status.ok());
    ASSERT_EQ(status.error_code(), grpc::StatusCode::NOT_FOUND);
}

TEST_F(KVStoreServerTest, EraseNotFound) {
    auto channel = grpc::CreateChannel(SERVER_ADDR, grpc::InsecureChannelCredentials());
    std::unique_ptr<zdb::kvStore::KVStoreService::Stub> stub = zdb::kvStore::KVStoreService::NewStub(channel);

    EraseRequest eraseReq;
    eraseReq.set_key("missing");
    EraseReply eraseRep;
    grpc::ClientContext ctx;
    auto status = stub->erase(&ctx, eraseReq, &eraseRep);
    ASSERT_FALSE(status.ok());
    ASSERT_EQ(status.error_code(), grpc::StatusCode::NOT_FOUND);
}

TEST_F(KVStoreServerTest, SizeEmpty) {
    auto channel = grpc::CreateChannel(SERVER_ADDR, grpc::InsecureChannelCredentials());
    std::unique_ptr<zdb::kvStore::KVStoreService::Stub> stub = zdb::kvStore::KVStoreService::NewStub(channel);

    const SizeRequest sizeReq;
    SizeReply sizeRep;
    grpc::ClientContext ctx;
    auto status = stub->size(&ctx, sizeReq, &sizeRep);
    ASSERT_TRUE(status.ok());
    ASSERT_EQ(sizeRep.size(), 0);
}

TEST_F(KVStoreServerTest, SizeNonEmpty) {
    auto channel = grpc::CreateChannel(SERVER_ADDR, grpc::InsecureChannelCredentials());
    std::unique_ptr<zdb::kvStore::KVStoreService::Stub> stub = zdb::kvStore::KVStoreService::NewStub(channel);

    SetRequest setReq;
    setReq.set_key("foo");
    setReq.set_value("bar");
    SetReply setRep;
    grpc::ClientContext ctx1;
    auto status = stub->set(&ctx1, setReq, &setRep);
    ASSERT_TRUE(status.ok());

    setReq.set_key("baz");
    setReq.set_value("qux");
    grpc::ClientContext ctx2;
    status = stub->set(&ctx2, setReq, &setRep);
    ASSERT_TRUE(status.ok());

    const SizeRequest sizeReq;
    SizeReply sizeRep;
    grpc::ClientContext ctx3;
    status = stub->size(&ctx3, sizeReq, &sizeRep);
    ASSERT_TRUE(status.ok());
    ASSERT_EQ(sizeRep.size(), 2);
}

TEST_F(KVStoreServerTest, SetEmptyKeyValue) {
    auto channel = grpc::CreateChannel(SERVER_ADDR, grpc::InsecureChannelCredentials());
    std::unique_ptr<zdb::kvStore::KVStoreService::Stub> stub = zdb::kvStore::KVStoreService::NewStub(channel);

    SetRequest setReq;
    setReq.set_key("");
    setReq.set_value("");
    SetReply setRep;
    grpc::ClientContext ctx;
    auto status = stub->set(&ctx, setReq, &setRep);
    // Accepts empty key/value unless server enforces otherwise
    ASSERT_TRUE(status.ok());
}

TEST_F(KVStoreServerTest, GetEmptyKey) {
    auto channel = grpc::CreateChannel(SERVER_ADDR, grpc::InsecureChannelCredentials());
    std::unique_ptr<zdb::kvStore::KVStoreService::Stub> stub = zdb::kvStore::KVStoreService::NewStub(channel);

    GetRequest getReq;
    getReq.set_key("");
    GetReply getRep;
    grpc::ClientContext ctx;
    auto status = stub->get(&ctx, getReq, &getRep);
    // Should return NOT_FOUND for empty key
    ASSERT_FALSE(status.ok());
    ASSERT_EQ(status.error_code(), grpc::StatusCode::NOT_FOUND);
}

TEST_F(KVStoreServerTest, EraseEmptyKey) {
    auto channel = grpc::CreateChannel(SERVER_ADDR, grpc::InsecureChannelCredentials());
    std::unique_ptr<zdb::kvStore::KVStoreService::Stub> stub = zdb::kvStore::KVStoreService::NewStub(channel);

    EraseRequest eraseReq;
    eraseReq.set_key("");
    EraseReply eraseRep;
    grpc::ClientContext ctx;
    auto status = stub->erase(&ctx, eraseReq, &eraseRep);
    ASSERT_FALSE(status.ok());
    ASSERT_EQ(status.error_code(), grpc::StatusCode::NOT_FOUND);
}
