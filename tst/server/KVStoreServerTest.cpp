/*
 * ZDB a distributed, fault-tolerant database.
 * Copyright (C) 2025 Ahmed Refaat Gadalla Mohamed
 *
 * This program is free software: you can redistribute it and/or modify it under the terms of the GNU Affero General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License along with this program. If not, see <https://www.gnu.org/licenses/>.
 */

#include <gtest/gtest.h>
#include <grpcpp/grpcpp.h>
#include "server/KVStoreServiceImpl.hpp"
#include "storage/InMemoryKVStore.hpp"
#include "proto/kvStore.grpc.pb.h"
#include <thread>
#include <chrono>
#include <string>
#include <memory>
#include "proto/kvStore.pb.h"
#include <grpcpp/support/status.h>
#include <grpcpp/security/credentials.h>
#include "common/Types.hpp"
#include "raft/TestRaft.hpp"
#include "raft/SyncChannel.hpp"
#include "common/KVStateMachine.hpp"

using zdb::Key;
using zdb::Value;
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
    raft::SyncChannel leader{};
    raft::SyncChannel follower{};
    TestRaft raft{leader};
    zdb::KVStateMachine kvState {kvStore, leader, follower, raft};
    KVStoreServiceImpl serviceImpl{kvState};
    std::unique_ptr<KVStoreServer> server;

    void SetUp() override {
        server = std::make_unique<KVStoreServer>(SERVER_ADDR, serviceImpl);
        // Wait for server to start
        std::this_thread::sleep_for(std::chrono::milliseconds{200L});
    }

    void TearDown() override {
        // Gracefully shutdown the server
        if (server) {
            server->shutdown();
        }
    }
};

TEST_F(KVStoreServerTest, SetAndGetSuccess) {
    auto channel = grpc::CreateChannel(SERVER_ADDR, grpc::InsecureChannelCredentials());
    std::unique_ptr<zdb::kvStore::KVStoreService::Stub> stub = zdb::kvStore::KVStoreService::NewStub(channel);

    SetRequest setReq;
    setReq.mutable_key()->set_data("foo");
    setReq.mutable_value()->set_data("bar");
    SetReply setRep;
    grpc::ClientContext ctx1;
    auto status = stub->set(&ctx1, setReq, &setRep);
    ASSERT_TRUE(status.ok());

    GetRequest getReq;
    getReq.mutable_key()->set_data("foo");
    GetReply getRep;
    auto ctx2 = grpc::ClientContext();
    status = stub->get(&ctx2, getReq, &getRep);
    ASSERT_TRUE(status.ok());
    ASSERT_EQ(getRep.value().data(), "bar");
}

TEST_F(KVStoreServerTest, GetNotFound) {
    auto channel = grpc::CreateChannel(SERVER_ADDR, grpc::InsecureChannelCredentials());
    std::unique_ptr<zdb::kvStore::KVStoreService::Stub> stub = zdb::kvStore::KVStoreService::NewStub(channel);

    GetRequest getReq;
    getReq.mutable_key()->set_data("missing");
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
    setReq.mutable_key()->set_data("foo");
    setReq.mutable_value()->set_data("bar");
    SetReply setRep;
    grpc::ClientContext ctx1;
    auto status = stub->set(&ctx1, setReq, &setRep);
    ASSERT_TRUE(status.ok());

    // Get the current value and its version
    GetRequest getReq1;
    getReq1.mutable_key()->set_data("foo");
    GetReply getRep1;
    grpc::ClientContext ctx_get;
    status = stub->get(&ctx_get, getReq1, &getRep1);
    ASSERT_TRUE(status.ok());
    ASSERT_EQ(getRep1.value().data(), "bar");

    // Overwrite with the correct version
    setReq.mutable_value()->set_data("baz");
    setReq.mutable_value()->set_version(getRep1.value().version());
    grpc::ClientContext ctx2;
    status = stub->set(&ctx2, setReq, &setRep);
    ASSERT_TRUE(status.ok());

    GetRequest getReq;
    getReq.mutable_key()->set_data("foo");
    GetReply getRep;
    grpc::ClientContext ctx3;
    status = stub->get(&ctx3, getReq, &getRep);
    ASSERT_TRUE(status.ok());
    ASSERT_EQ(getRep.value().data(), "baz");
}

TEST_F(KVStoreServerTest, EraseSuccess) {
    auto channel = grpc::CreateChannel(SERVER_ADDR, grpc::InsecureChannelCredentials());
    std::unique_ptr<zdb::kvStore::KVStoreService::Stub> stub = zdb::kvStore::KVStoreService::NewStub(channel);

    SetRequest setReq;
    setReq.mutable_key()->set_data("foo");
    setReq.mutable_value()->set_data("bar");
    SetReply setRep;
    grpc::ClientContext ctx1;
    auto status = stub->set(&ctx1, setReq, &setRep);
    ASSERT_TRUE(status.ok());

    EraseRequest eraseReq;
    eraseReq.mutable_key()->set_data("foo");
    EraseReply eraseRep;
    grpc::ClientContext ctx2;
    status = stub->erase(&ctx2, eraseReq, &eraseRep);
    ASSERT_TRUE(status.ok());
    ASSERT_EQ(eraseRep.value().data(), "bar");

    GetRequest getReq;
    getReq.mutable_key()->set_data("foo");
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
    eraseReq.mutable_key()->set_data("missing");
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
    setReq.mutable_key()->set_data("foo");
    setReq.mutable_value()->set_data("bar");
    SetReply setRep;
    grpc::ClientContext ctx1;
    auto status = stub->set(&ctx1, setReq, &setRep);
    ASSERT_TRUE(status.ok());

    setReq.mutable_key()->set_data("baz");
    setReq.mutable_value()->set_data("qux");
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
    setReq.mutable_key()->set_data("");
    setReq.mutable_value()->set_data("");
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
    getReq.mutable_key()->set_data("");
    GetReply getRep;
    grpc::ClientContext ctx;
    auto status = stub->get(&ctx, getReq, &getRep);
    ASSERT_FALSE(status.ok());
    ASSERT_EQ(status.error_code(), grpc::StatusCode::NOT_FOUND);
}

TEST_F(KVStoreServerTest, EraseEmptyKey) {
    auto channel = grpc::CreateChannel(SERVER_ADDR, grpc::InsecureChannelCredentials());
    std::unique_ptr<zdb::kvStore::KVStoreService::Stub> stub = zdb::kvStore::KVStoreService::NewStub(channel);

    EraseRequest eraseReq;
    eraseReq.mutable_key()->set_data("");
    EraseReply eraseRep;
    grpc::ClientContext ctx;
    auto status = stub->erase(&ctx, eraseReq, &eraseRep);
    ASSERT_FALSE(status.ok());
    ASSERT_EQ(status.error_code(), grpc::StatusCode::NOT_FOUND);
}
