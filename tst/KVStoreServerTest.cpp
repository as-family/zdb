
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

using namespace zdb;

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
    std::unique_ptr<kvStore::KVStoreService::Stub> stub = kvStore::KVStoreService::NewStub(channel);

    kvStore::SetRequest setReq;
    setReq.set_key("foo");
    setReq.set_value("bar");
    kvStore::SetReply setRep;
    grpc::ClientContext ctx1;
    auto status = stub->set(&ctx1, setReq, &setRep);
    ASSERT_TRUE(status.ok());

    kvStore::GetRequest getReq;
    getReq.set_key("foo");
    kvStore::GetReply getRep;
    auto ctx2 = grpc::ClientContext();
    status = stub->get(&ctx2, getReq, &getRep);
    ASSERT_TRUE(status.ok());
    ASSERT_EQ(getRep.value(), "bar");
}

TEST_F(KVStoreServerTest, GetNotFound) {
    auto channel = grpc::CreateChannel(SERVER_ADDR, grpc::InsecureChannelCredentials());
    std::unique_ptr<kvStore::KVStoreService::Stub> stub = kvStore::KVStoreService::NewStub(channel);

    kvStore::GetRequest getReq;
    getReq.set_key("missing");
    kvStore::GetReply getRep;
    grpc::ClientContext ctx;
    auto status = stub->get(&ctx, getReq, &getRep);
    ASSERT_FALSE(status.ok());
    ASSERT_EQ(status.error_code(), grpc::StatusCode::NOT_FOUND);
}

TEST_F(KVStoreServerTest, SetOverwrite) {
    auto channel = grpc::CreateChannel(SERVER_ADDR, grpc::InsecureChannelCredentials());
    std::unique_ptr<kvStore::KVStoreService::Stub> stub = kvStore::KVStoreService::NewStub(channel);

    kvStore::SetRequest setReq;
    setReq.set_key("foo");
    setReq.set_value("bar");
    kvStore::SetReply setRep;
    grpc::ClientContext ctx1;
    auto status = stub->set(&ctx1, setReq, &setRep);
    ASSERT_TRUE(status.ok());

    setReq.set_value("baz");
    grpc::ClientContext ctx2;
    status = stub->set(&ctx2, setReq, &setRep);
    ASSERT_TRUE(status.ok());

    kvStore::GetRequest getReq;
    getReq.set_key("foo");
    kvStore::GetReply getRep;
    grpc::ClientContext ctx3;
    status = stub->get(&ctx3, getReq, &getRep);
    ASSERT_TRUE(status.ok());
    ASSERT_EQ(getRep.value(), "baz");
}

TEST_F(KVStoreServerTest, EraseSuccess) {
    auto channel = grpc::CreateChannel(SERVER_ADDR, grpc::InsecureChannelCredentials());
    std::unique_ptr<kvStore::KVStoreService::Stub> stub = kvStore::KVStoreService::NewStub(channel);

    kvStore::SetRequest setReq;
    setReq.set_key("foo");
    setReq.set_value("bar");
    kvStore::SetReply setRep;
    grpc::ClientContext ctx1;
    auto status = stub->set(&ctx1, setReq, &setRep);
    ASSERT_TRUE(status.ok());

    kvStore::EraseRequest eraseReq;
    eraseReq.set_key("foo");
    kvStore::EraseReply eraseRep;
    grpc::ClientContext ctx2;
    status = stub->erase(&ctx2, eraseReq, &eraseRep);
    ASSERT_TRUE(status.ok());
    ASSERT_EQ(eraseRep.value(), "bar");

    kvStore::GetRequest getReq;
    getReq.set_key("foo");
    kvStore::GetReply getRep;
    grpc::ClientContext ctx3;
    status = stub->get(&ctx3, getReq, &getRep);
    ASSERT_FALSE(status.ok());
    ASSERT_EQ(status.error_code(), grpc::StatusCode::NOT_FOUND);
}

TEST_F(KVStoreServerTest, EraseNotFound) {
    auto channel = grpc::CreateChannel(SERVER_ADDR, grpc::InsecureChannelCredentials());
    std::unique_ptr<kvStore::KVStoreService::Stub> stub = kvStore::KVStoreService::NewStub(channel);

    kvStore::EraseRequest eraseReq;
    eraseReq.set_key("missing");
    kvStore::EraseReply eraseRep;
    grpc::ClientContext ctx;
    auto status = stub->erase(&ctx, eraseReq, &eraseRep);
    ASSERT_FALSE(status.ok());
    ASSERT_EQ(status.error_code(), grpc::StatusCode::NOT_FOUND);
}

TEST_F(KVStoreServerTest, SizeEmpty) {
    auto channel = grpc::CreateChannel(SERVER_ADDR, grpc::InsecureChannelCredentials());
    std::unique_ptr<kvStore::KVStoreService::Stub> stub = kvStore::KVStoreService::NewStub(channel);

    const kvStore::SizeRequest sizeReq;
    kvStore::SizeReply sizeRep;
    grpc::ClientContext ctx;
    auto status = stub->size(&ctx, sizeReq, &sizeRep);
    ASSERT_TRUE(status.ok());
    ASSERT_EQ(sizeRep.size(), 0);
}

TEST_F(KVStoreServerTest, SizeNonEmpty) {
    auto channel = grpc::CreateChannel(SERVER_ADDR, grpc::InsecureChannelCredentials());
    std::unique_ptr<kvStore::KVStoreService::Stub> stub = kvStore::KVStoreService::NewStub(channel);

    kvStore::SetRequest setReq;
    setReq.set_key("foo");
    setReq.set_value("bar");
    kvStore::SetReply setRep;
    grpc::ClientContext ctx1;
    auto status = stub->set(&ctx1, setReq, &setRep);
    ASSERT_TRUE(status.ok());

    setReq.set_key("baz");
    setReq.set_value("qux");
    grpc::ClientContext ctx2;
    status = stub->set(&ctx2, setReq, &setRep);
    ASSERT_TRUE(status.ok());

    const kvStore::SizeRequest sizeReq;
    kvStore::SizeReply sizeRep;
    grpc::ClientContext ctx3;
    status = stub->size(&ctx3, sizeReq, &sizeRep);
    ASSERT_TRUE(status.ok());
    ASSERT_EQ(sizeRep.size(), 2);
}

TEST_F(KVStoreServerTest, SetEmptyKeyValue) {
    auto channel = grpc::CreateChannel(SERVER_ADDR, grpc::InsecureChannelCredentials());
    std::unique_ptr<kvStore::KVStoreService::Stub> stub = kvStore::KVStoreService::NewStub(channel);

    kvStore::SetRequest setReq;
    setReq.set_key("");
    setReq.set_value("");
    kvStore::SetReply setRep;
    grpc::ClientContext ctx;
    auto status = stub->set(&ctx, setReq, &setRep);
    // Accepts empty key/value unless server enforces otherwise
    ASSERT_TRUE(status.ok());
}

TEST_F(KVStoreServerTest, GetEmptyKey) {
    auto channel = grpc::CreateChannel(SERVER_ADDR, grpc::InsecureChannelCredentials());
    std::unique_ptr<kvStore::KVStoreService::Stub> stub = kvStore::KVStoreService::NewStub(channel);

    kvStore::GetRequest getReq;
    getReq.set_key("");
    kvStore::GetReply getRep;
    grpc::ClientContext ctx;
    auto status = stub->get(&ctx, getReq, &getRep);
    // Should return NOT_FOUND for empty key
    ASSERT_FALSE(status.ok());
    ASSERT_EQ(status.error_code(), grpc::StatusCode::NOT_FOUND);
}

TEST_F(KVStoreServerTest, EraseEmptyKey) {
    auto channel = grpc::CreateChannel(SERVER_ADDR, grpc::InsecureChannelCredentials());
    std::unique_ptr<kvStore::KVStoreService::Stub> stub = kvStore::KVStoreService::NewStub(channel);

    kvStore::EraseRequest eraseReq;
    eraseReq.set_key("");
    kvStore::EraseReply eraseRep;
    grpc::ClientContext ctx;
    auto status = stub->erase(&ctx, eraseReq, &eraseRep);
    ASSERT_FALSE(status.ok());
    ASSERT_EQ(status.error_code(), grpc::StatusCode::NOT_FOUND);
}
