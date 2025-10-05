// SPDX-License-Identifier: AGPL-3.0-or-later
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
#include <gmock/gmock.h>
#include "raft/RaftImpl.hpp"
#include "raft/SyncChannel.hpp"
#include "raft/Types.hpp"
#include "raft/Log.hpp"
#include "common/RetryPolicy.hpp"
#include "common/Error.hpp"
#include "proto/raft.grpc.pb.h"
#include <chrono>
#include <memory>
#include <string>
#include <vector>
#include <optional>
#include <functional>
#include <unordered_map>
#include <type_traits>
#include <tuple>
#include "common/Command.hpp"
#include "common/TypesMap.hpp"
#include "storage/Persister.hpp"
#include "storage/FilePersister.hpp"

using namespace raft;
using namespace testing;

// Mock Client for testing RaftImpl without actual network calls
class MockClient {
public:
    MockClient(const std::string& addr, const zdb::RetryPolicy& retryPolicy)
        : address(addr), policy(retryPolicy) {}
    
    // Mock for call method - returns success result with optional wrapping
    template<typename Req, typename Rep = zdb::map_to_t<Req>>
    std::optional<Rep> call(const std::string& /* op */,
                       const Req& /* request */) {
        if constexpr (std::is_same_v<Rep, RequestVoteReply>) {
            Rep reply;
            reply.voteGranted = true;
            reply.term = 1;
            return reply;
        } else if constexpr (std::is_same_v<Rep, AppendEntriesReply>) {
            Rep reply;
            reply.success = true;
            reply.term = 1;
            return reply;
        }
        return std::nullopt;
    }

private:
    std::string address;
    zdb::RetryPolicy policy;
};

// Mock Channel for testing
class MockChannel final : public Channel<std::shared_ptr<raft::Command>> {
public:
    MOCK_METHOD(void, send, (std::shared_ptr<raft::Command> message), (override));
    MOCK_METHOD(bool, sendUntil, (std::shared_ptr<raft::Command>, std::chrono::system_clock::time_point t), (override));
    MOCK_METHOD(std::optional<std::shared_ptr<raft::Command>>, receive, (), (override));
    MOCK_METHOD(std::optional<std::shared_ptr<raft::Command>>, receiveUntil, (std::chrono::system_clock::time_point t), (override));
    MOCK_METHOD(void, close, (), (override));
    MOCK_METHOD(bool, isClosed, (), (override));
};

class RaftImplTest : public ::testing::Test {
protected:
    void SetUp() override {
        peers = {"peer1", "peer2"};
        selfId = "self";
        
        policy = zdb::RetryPolicy{
            std::chrono::microseconds{10L},    // baseDelay
            std::chrono::microseconds{100L},   // maxDelay
            std::chrono::microseconds{1000L},  // resetTimeout
            3,                                // failureThreshold
            1,                                // servicesToTry
            std::chrono::milliseconds{1000L},  // rpcTimeout
            std::chrono::milliseconds{200L}    // channelTimeout
        };
        
        serviceChannel = std::make_unique<MockChannel>();

        // Setup expectations for channels to not be used in basic constructor tests
        EXPECT_CALL(*serviceChannel, isClosed()).WillRepeatedly(Return(false));

        clientFactory = [this](const std::string& addr, const zdb::RetryPolicy& retryPolicy, std::atomic<bool>& sc) -> MockClient& {
            auto client = std::make_unique<MockClient>(addr, retryPolicy);
            MockClient* clientPtr = client.get();
            mockClients[addr] = std::move(client);
            return *clientPtr;
        };
    }

    std::vector<std::string> peers;
    std::string selfId;
    zdb::RetryPolicy policy{
        std::chrono::microseconds{10L},
        std::chrono::microseconds{100L},
        std::chrono::microseconds{1000L},
        3, 1,
        std::chrono::milliseconds{1000L},
        std::chrono::milliseconds{200L}
    };
    std::unique_ptr<MockChannel> serviceChannel;
    std::function<MockClient&(const std::string&, const zdb::RetryPolicy&, std::atomic<bool>& sc)> clientFactory;
    std::unordered_map<std::string, std::unique_ptr<MockClient>> mockClients;
    zdb::FilePersister persister = zdb::FilePersister{"."};
};

TEST_F(RaftImplTest, ConstructorInitializesCorrectly) {
    // Test that constructor initializes the Raft instance correctly
    auto raft = std::make_unique<raft::RaftImpl<MockClient>>(
        peers, selfId, *serviceChannel, policy, clientFactory, persister);
    
    EXPECT_EQ(raft->getRole(), raft::Role::Follower);
    EXPECT_EQ(raft->getSelfId(), selfId);
    EXPECT_EQ(raft->getCurrentTerm(), 0);
    
    // Verify log is initialized
    EXPECT_EQ(raft->log().lastIndex(), 0);
}

TEST_F(RaftImplTest, StartCommandWhenFollower) {
    auto raft = std::make_unique<RaftImpl<MockClient>>(
        peers, selfId, *serviceChannel, policy, clientFactory, persister);
    
    // Follower should not accept start commands
    EXPECT_FALSE(raft->start(nullptr));
}

TEST_F(RaftImplTest, StartCommandWhenLeader) {
    auto raft = std::make_unique<RaftImpl<MockClient>>(
        peers, selfId, *serviceChannel, policy, clientFactory, persister);
    
    // Manually set role to leader for testing
    // Note: This requires accessing protected members or using friend class
    // For now, we'll test the behavior through request vote simulation
    
        // Mock successful append entries calls for all peers
        for (const auto& peer : peers) {
            // For unit testing, we assume successful calls
            // In real implementation, this would be handled by network layer
            std::ignore = peer; // Suppress unused variable warning
        }    // Test will be expanded when we can properly set leader state
}

TEST_F(RaftImplTest, RequestVoteHandlerWithHigherTerm) {
    auto raft = std::make_unique<RaftImpl<MockClient>>(
        peers, selfId, *serviceChannel, policy, clientFactory, persister);
    
    proto::RequestVoteArg protoArg;
    protoArg.set_candidateid("candidate1");
    protoArg.set_term(5);  // Higher than current term (0)
    protoArg.set_lastlogindex(0);
    protoArg.set_lastlogterm(0);
    
    RequestVoteArg arg{protoArg};
    RequestVoteReply reply = raft->requestVoteHandler(arg);
    
    // As per Raft spec: if term > currentTerm, update currentTerm and convert to follower
    EXPECT_TRUE(reply.voteGranted);
    EXPECT_EQ(raft->getCurrentTerm(), 5);  // Term should be updated
    EXPECT_EQ(raft->getRole(), raft::Role::Follower);  // Should remain/become follower
}

TEST_F(RaftImplTest, RequestVoteHandlerWithLowerTerm) {
    auto raft = std::make_unique<RaftImpl<MockClient>>(
        peers, selfId, *serviceChannel, policy, clientFactory, persister);
    
    // First set current term to 10
    proto::RequestVoteArg setupArg;
    setupArg.set_candidateid("setup");
    setupArg.set_term(10);
    setupArg.set_lastlogindex(0);
    setupArg.set_lastlogterm(0);
    RequestVoteArg setup{setupArg};
    raft->requestVoteHandler(setup);
    
    // Now test with lower term - should be rejected per Raft spec
    proto::RequestVoteArg protoArg;
    protoArg.set_candidateid("candidate1");
    protoArg.set_term(5);  // Lower than current term (10)
    protoArg.set_lastlogindex(0);
    protoArg.set_lastlogterm(0);
    
    RequestVoteArg arg{protoArg};
    RequestVoteReply reply = raft->requestVoteHandler(arg);
    
    // As per Raft spec: if term < currentTerm, reject vote and return current term
    EXPECT_FALSE(reply.voteGranted);
    EXPECT_EQ(raft->getCurrentTerm(), 10);  // Term should not change
}

TEST_F(RaftImplTest, RequestVoteHandlerAlreadyVoted) {
    auto raft = std::make_unique<RaftImpl<MockClient>>(
        peers, selfId, *serviceChannel, policy, clientFactory, persister);
    
    // First vote in term 5
    proto::RequestVoteArg protoArg1;
    protoArg1.set_candidateid("candidate1");
    protoArg1.set_term(5);
    protoArg1.set_lastlogindex(0);
    protoArg1.set_lastlogterm(0);
    
    RequestVoteArg arg1{protoArg1};
    RequestVoteReply reply1 = raft->requestVoteHandler(arg1);
    EXPECT_TRUE(reply1.voteGranted);
    
    // Second vote for different candidate in same term - should be rejected
    // As per Raft spec: can only vote for one candidate per term
    proto::RequestVoteArg protoArg2;
    protoArg2.set_candidateid("candidate2");
    protoArg2.set_term(5);  // Same term
    protoArg2.set_lastlogindex(0);
    protoArg2.set_lastlogterm(0);
    
    RequestVoteArg arg2{protoArg2};
    RequestVoteReply reply2 = raft->requestVoteHandler(arg2);
    EXPECT_FALSE(reply2.voteGranted);  // Should reject second vote
    
    // But voting for same candidate again in same term should succeed
    RequestVoteReply reply3 = raft->requestVoteHandler(arg1);
    EXPECT_TRUE(reply3.voteGranted);  // Should accept vote for same candidate
}

TEST_F(RaftImplTest, AppendEntriesHandlerWithHigherTerm) {
    auto raft = std::make_unique<RaftImpl<MockClient>>(
        peers, selfId, *serviceChannel, policy, clientFactory, persister);
    
    Log emptyLog;
    AppendEntriesArg arg{"leader1", 5, 0, 0, 0, emptyLog};
    
    // Expect followerChannel to receive applied entries
    EXPECT_CALL(*serviceChannel, sendUntil(_, _)).Times(0); // No entries to apply
    
    AppendEntriesReply reply = raft->appendEntriesHandler(arg);
    
    EXPECT_TRUE(reply.success);
    EXPECT_EQ(reply.term, 5);
    EXPECT_EQ(raft->getCurrentTerm(), 5);
}

// Raft spec: If term < currentTerm, reply false
TEST_F(RaftImplTest, AppendEntriesHandlerWithLowerTerm) {
    auto raft = std::make_unique<RaftImpl<MockClient>>(
        peers, selfId, *serviceChannel, policy, clientFactory, persister);
    
    // Set current term to 10
    proto::RequestVoteArg setupArg;
    setupArg.set_candidateid("setup");
    setupArg.set_term(10);
    setupArg.set_lastlogindex(0);
    setupArg.set_lastlogterm(0);
    RequestVoteArg setup{setupArg};
    raft->requestVoteHandler(setup);
    
    // Test append entries with lower term - Raft spec violation
    Log emptyLog;
    AppendEntriesArg arg{"leader1", 5, 0, 0, 0, emptyLog};
    
    AppendEntriesReply reply = raft->appendEntriesHandler(arg);
    
    // Raft spec: Reply false if term < currentTerm (§5.1)
    EXPECT_FALSE(reply.success);
    EXPECT_EQ(reply.term, 10);
    EXPECT_EQ(raft->getCurrentTerm(), 10); // Term should not decrease
}

// Raft spec: Reply false if log doesn't contain entry at prevLogIndex 
TEST_F(RaftImplTest, AppendEntriesHandlerLogInconsistency) {
    auto raft = std::make_unique<RaftImpl<MockClient>>(
        peers, selfId, *serviceChannel, policy, clientFactory, persister);
    
    Log emptyLog;
    // Try to append at index 5 when log is empty (prevLogIndex doesn't exist)
    AppendEntriesArg arg{"leader1", 5, 5, 1, 0, emptyLog};
    
    AppendEntriesReply reply = raft->appendEntriesHandler(arg);
    
    // Raft spec: Reply false if log doesn't contain an entry at prevLogIndex (§5.3)
    EXPECT_FALSE(reply.success);
    EXPECT_EQ(reply.term, 5); // Term gets updated for higher term
    EXPECT_EQ(raft->getCurrentTerm(), 5); // But accept higher term
}

TEST_F(RaftImplTest, LogAccessor) {
    auto raft = std::make_unique<RaftImpl<MockClient>>(
        peers, selfId, *serviceChannel, policy, clientFactory, persister);
    
    Log& log = raft->log();
    EXPECT_EQ(log.lastIndex(), 0);
    
    // Add an entry directly to test log access
    auto u = generate_uuid_v7();
    LogEntry entry{1, 1, std::make_unique<zdb::Get>(u, zdb::Key{"k"})};
    log.append(entry);
    EXPECT_EQ(log.lastIndex(), 1);
}

TEST_F(RaftImplTest, KillMethod) {
    auto raft = std::make_unique<RaftImpl<MockClient>>(
        peers, selfId, *serviceChannel, policy, clientFactory, persister);
    
    // Kill should set internal killed flag
    raft->kill();
    
    // After kill, start should still work if it was a leader (behavior test)
    // This is more of a state consistency test
    auto u = generate_uuid_v7();
    EXPECT_FALSE(raft->start(std::make_unique<zdb::Get>(u, zdb::Key{"command-after-kill"})));
}

// Raft spec: Append entries and update commitIndex
TEST_F(RaftImplTest, AppendEntriesWithValidEntries) {
    auto raft = std::make_unique<RaftImpl<MockClient>>(
        peers, selfId, *serviceChannel, policy, clientFactory, persister);
    
    // Create a log with one entry
    Log entriesLog;
    auto u = generate_uuid_v7();
    LogEntry entry{1, 1, std::make_unique<zdb::Get>(u, zdb::Key{"test-command"})};
    entriesLog.append(entry);
    
    // Expect the command to be sent to followerChannel when applied
    EXPECT_CALL(*serviceChannel, sendUntil(_, _)).Times(1);

    AppendEntriesArg arg{"leader1", 1, 0, 0, 1, entriesLog}; // leaderCommit = 1
    AppendEntriesReply reply = raft->appendEntriesHandler(arg);
    
    // Raft spec: If existing entry conflicts, delete it and all following entries (§5.3)
    // Raft spec: Append any new entries not already in the log
    EXPECT_TRUE(reply.success);
    EXPECT_EQ(reply.term, 1);
    EXPECT_EQ(raft->log().lastIndex(), 1);
    // State Machine Safety: commands applied when committed
}

// Test Raft Log Matching Property - log consistency across multiple appends
TEST_F(RaftImplTest, MultipleAppendEntriesOperations) {
    auto raft = std::make_unique<RaftImpl<MockClient>>(
        peers, selfId, *serviceChannel, policy, clientFactory, persister);
    
    // First append
    Log entriesLog1;
    auto u1 = generate_uuid_v7();
    LogEntry entry1{1, 1, std::make_unique<zdb::Get>(u1, zdb::Key{"command1"})};
    entriesLog1.append(entry1);
    
    EXPECT_CALL(*serviceChannel, sendUntil(_, _)).Times(1).WillOnce(Return(true));

    AppendEntriesArg arg1{"leader1", 1, 0, 0, 1, entriesLog1};
    AppendEntriesReply reply1 = raft->appendEntriesHandler(arg1);
    EXPECT_TRUE(reply1.success);
    
    // Second append (should build on first) - tests Log Matching Property
    Log entriesLog2;
    auto u2 = generate_uuid_v7();
    LogEntry entry2{2, 1, std::make_unique<zdb::Set>(u2, zdb::Key{"command2"}, zdb::Value{"value2"})};
    entriesLog2.append(entry2);
    
    // On second append, only command2 should be applied since command1 is already applied
    EXPECT_CALL(*serviceChannel, sendUntil(_, _)).Times(1).WillOnce(Return(true));

    AppendEntriesArg arg2{"leader1", 1, 1, 1, 2, entriesLog2}; // prevLogIndex=1, prevLogTerm=1
    AppendEntriesReply reply2 = raft->appendEntriesHandler(arg2);
    EXPECT_TRUE(reply2.success);
    
    // Raft Log Matching Property: logs are consistent
    EXPECT_EQ(raft->log().lastIndex(), 2);
    // If logs contain entries with same index & term, they're identical (§5.3)
}

TEST_F(RaftImplTest, RequestVoteHandlerOutdatedLog) {
    auto raft = std::make_unique<RaftImpl<MockClient>>(
        peers, selfId, *serviceChannel, policy, clientFactory, persister);
    
    // Add some entries to the log to make it more up-to-date
    auto u1 = generate_uuid_v7();
    auto u2 = generate_uuid_v7();
    LogEntry entry1{1, 1, std::make_unique<zdb::Get>(u1, zdb::Key{"command1"})};
    LogEntry entry2{2, 2, std::make_unique<zdb::Set>(u2, zdb::Key{"command2"}, zdb::Value{"value2"})};
    raft->log().append(entry1);
    raft->log().append(entry2);
    
    // Request vote with outdated log (term 1, index 1 vs our term 2, index 2)
    // As per Raft spec: reject vote if candidate's log is less up-to-date
    proto::RequestVoteArg protoArg;
    protoArg.set_candidateid("candidate1");
    protoArg.set_term(3); // Higher term but outdated log
    protoArg.set_lastlogindex(1);
    protoArg.set_lastlogterm(1);
    
    RequestVoteArg arg{protoArg};
    RequestVoteReply reply = raft->requestVoteHandler(arg);
    
    // Should reject due to outdated log per Raft spec Section 5.4.1
    EXPECT_FALSE(reply.voteGranted); 
    EXPECT_EQ(raft->getCurrentTerm(), 3); // Term should be updated even if vote rejected
}

// Test for Raft Log Matching Property
TEST_F(RaftImplTest, LogMatchingProperty) {
    auto raft = std::make_unique<RaftImpl<MockClient>>(
        peers, selfId, *serviceChannel, policy, clientFactory, persister);
    
    // Add entries to establish a log
    auto u1 = generate_uuid_v7();
    auto u2 = generate_uuid_v7();
    LogEntry entry1{1, 1, std::make_unique<zdb::Get>(u1, zdb::Key{"command1"})};
    LogEntry entry2{2, 2, std::make_unique<zdb::Set>(u2, zdb::Key{"command2"}, zdb::Value{"value2"})};
    raft->log().append(entry1);
    raft->log().append(entry2);
    
    // Test successful append with matching prevLogIndex and prevLogTerm
    Log newEntries;
    auto u3 = generate_uuid_v7();
    LogEntry entry3{3, 2, std::make_unique<zdb::Erase>(u3, zdb::Key{"command3"})};
    newEntries.append(entry3);
    
    // This should succeed: prevLogIndex=2, prevLogTerm=2 matches our log
    AppendEntriesArg matchingArg{"leader1", 2, 2, 2, 0, newEntries};
    AppendEntriesReply reply1 = raft->appendEntriesHandler(matchingArg);
    EXPECT_TRUE(reply1.success);
    
    // Test failed append with non-matching prevLogTerm
    Log conflictEntries;
    auto u4 = generate_uuid_v7();
    LogEntry entry4{4, 3, std::make_unique<zdb::Size>(u4)};
    conflictEntries.append(entry4);
    
    // This should fail: prevLogIndex=2 exists but prevLogTerm=1 doesn't match (should be 2)
    AppendEntriesArg conflictArg{"leader1", 3, 2, 1, 0, conflictEntries};
    AppendEntriesReply reply2 = raft->appendEntriesHandler(conflictArg);
    EXPECT_FALSE(reply2.success);  // Log Matching Property violation
}

// Test for Raft Safety Properties
TEST_F(RaftImplTest, LeaderAppendOnlyProperty) {
    auto raft = std::make_unique<RaftImpl<MockClient>>(
        peers, selfId, *serviceChannel, policy, clientFactory, persister);
    
    // Add some entries to log
    auto u1 = generate_uuid_v7();
    auto u2 = generate_uuid_v7();
    LogEntry entry1{1, 1, std::make_unique<zdb::Get>(u1, zdb::Key{"command1"})};
    LogEntry entry2{2, 1, std::make_unique<zdb::Set>(u2, zdb::Key{"command2"}, zdb::Value{"value2"})};
    raft->log().append(entry1);
    raft->log().append(entry2);
    
    uint64_t originalLogSize = raft->log().lastIndex();
    
    // When receiving AppendEntries with matching prefix, should not delete existing entries
    Log appendLog;
    auto u3 = generate_uuid_v7();
    LogEntry entry3{3, 2, std::make_unique<zdb::Erase>(u3, zdb::Key{"command3"})};
    appendLog.append(entry3);
    
    AppendEntriesArg arg{"leader1", 2, 2, 1, 0, appendLog}; // prevLogIndex=2 matches existing
    AppendEntriesReply reply = raft->appendEntriesHandler(arg);
    
    EXPECT_TRUE(reply.success);
    EXPECT_GE(raft->log().lastIndex(), originalLogSize); // Log should only grow, never shrink
}

// Test Raft Election Safety - at most one leader per term
TEST_F(RaftImplTest, ElectionSafetyProperty) {
    auto raft = std::make_unique<RaftImpl<MockClient>>(
        peers, selfId, *serviceChannel, policy, clientFactory, persister);
    
    // Test that a follower cannot grant vote to multiple candidates in same term
    proto::RequestVoteArg vote1;
    vote1.set_candidateid("candidate1");
    vote1.set_term(5);
    vote1.set_lastlogindex(0);
    vote1.set_lastlogterm(0);
    
    RequestVoteArg arg1{vote1};
    RequestVoteReply reply1 = raft->requestVoteHandler(arg1);
    EXPECT_TRUE(reply1.voteGranted);
    
    // Different candidate in same term should be rejected
    proto::RequestVoteArg vote2;
    vote2.set_candidateid("candidate2");
    vote2.set_term(5); // Same term
    vote2.set_lastlogindex(0);
    vote2.set_lastlogterm(0);
    
    RequestVoteArg arg2{vote2};
    RequestVoteReply reply2 = raft->requestVoteHandler(arg2);
    EXPECT_FALSE(reply2.voteGranted); // Election Safety: can't vote for two candidates
}

// Test State Machine Safety Property
TEST_F(RaftImplTest, StateMachineSafetyProperty) {
    auto raft = std::make_unique<RaftImpl<MockClient>>(
        peers, selfId, *serviceChannel, policy, clientFactory, persister);
    
    // Test that committed entries are applied in order
    Log entriesLog;
    auto u1 = generate_uuid_v7();
    auto u2 = generate_uuid_v7();
    LogEntry entry1{1, 1, std::make_unique<zdb::Get>(u1, zdb::Key{"command1"})};
    LogEntry entry2{2, 1, std::make_unique<zdb::Set>(u2, zdb::Key{"command2"}, zdb::Value{"value2"})};
    entriesLog.append(entry1);
    entriesLog.append(entry2);
    
    // Expect commands to be applied in order when both are committed
    InSequence seq;
    EXPECT_CALL(*serviceChannel, sendUntil(_, _)).Times(1).WillOnce(Return(true));
    EXPECT_CALL(*serviceChannel, sendUntil(_, _)).Times(1).WillOnce(Return(true));

    // Leader commits both entries (leaderCommit = 2)
    AppendEntriesArg arg{"leader1", 1, 0, 0, 2, entriesLog};
    AppendEntriesReply reply = raft->appendEntriesHandler(arg);
    
    EXPECT_TRUE(reply.success);
    // State machine safety: entries applied in correct order
}

// Test for term monotonicity - terms never decrease
TEST_F(RaftImplTest, TermMonotonicityProperty) {
    auto raft = std::make_unique<RaftImpl<MockClient>>(
        peers, selfId, *serviceChannel, policy, clientFactory, persister);
    
    EXPECT_EQ(raft->getCurrentTerm(), 0);
    
    // Receive message with term 5
    proto::RequestVoteArg protoArg;
    protoArg.set_candidateid("candidate1");
    protoArg.set_term(5);
    protoArg.set_lastlogindex(0);
    protoArg.set_lastlogterm(0);
    
    RequestVoteArg arg{protoArg};
    raft->requestVoteHandler(arg);
    EXPECT_EQ(raft->getCurrentTerm(), 5);
    
    // Receive message with higher term 7
    protoArg.set_term(7);
    RequestVoteArg arg2{protoArg};
    raft->requestVoteHandler(arg2);
    EXPECT_EQ(raft->getCurrentTerm(), 7);
    
    // Receive message with lower term 3 - should not decrease current term
    protoArg.set_term(3);
    RequestVoteArg arg3{protoArg};
    raft->requestVoteHandler(arg3);
    EXPECT_EQ(raft->getCurrentTerm(), 7); // Term should not decrease
}

class RaftImplIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        policy = zdb::RetryPolicy{
            std::chrono::microseconds{10L},
            std::chrono::microseconds{100L},
            std::chrono::microseconds{1000L},
            3, 1,
            std::chrono::milliseconds{1000L},
            std::chrono::milliseconds{200L}
        };
        
        serviceChannel = std::make_unique<raft::SyncChannel<std::shared_ptr<raft::Command>>>();
    }
    
    zdb::RetryPolicy policy{
        std::chrono::microseconds{10L},
        std::chrono::microseconds{100L},
        std::chrono::microseconds{1000L},
        3, 1,
        std::chrono::milliseconds{1000L},
        std::chrono::milliseconds{200L}
    };
    std::unique_ptr<raft::SyncChannel<std::shared_ptr<raft::Command>>> serviceChannel;
    zdb::FilePersister persister = zdb::FilePersister{"."};
};

TEST_F(RaftImplIntegrationTest, BasicChannelInteraction) {
    std::vector<std::string> peers = {"peer1"};
    std::string selfId = "self";
    
    auto clientFactory = [](const std::string& addr, const zdb::RetryPolicy& retryPolicy, std::atomic<bool>&) -> MockClient& {
        static MockClient client(addr, retryPolicy);
        return client;
    };
    
    auto raft = std::make_unique<RaftImpl<MockClient>>(
        peers, selfId, *serviceChannel, policy, clientFactory, persister);
    
    // Test basic log entry and commit
    Log entriesLog;
    auto u = generate_uuid_v7();
    LogEntry entry{1, 1, std::make_unique<zdb::Get>(u, zdb::Key{"test-command"})};
    entriesLog.append(entry);
    
    AppendEntriesArg arg{"leader1", 1, 0, 0, 1, entriesLog};
    AppendEntriesReply reply = raft->appendEntriesHandler(arg);
    
    EXPECT_TRUE(reply.success);
    
    // Check that command was sent to follower channel
    // Note: Due to async nature, we might need to wait or use polling
}

TEST_F(RaftImplIntegrationTest, StateTransitions) {
    std::vector<std::string> peers = {};  // Single node cluster
    std::string selfId = "self";
    
    auto clientFactory = [](const std::string& addr, const zdb::RetryPolicy& retryPolicy, std::atomic<bool>&) -> MockClient& {
        static MockClient client(addr, retryPolicy);
        return client;
    };
    
    auto raft = std::make_unique<RaftImpl<MockClient>>(
        peers, selfId, *serviceChannel, policy, clientFactory, persister);
    
    // Start as follower
    EXPECT_EQ(raft->getRole(), Role::Follower);
    
    // In a single-node cluster, should be able to become leader quickly
    // This test would require waiting for election timeout and checking state changes
    // For unit test, we'll just verify initial state is correct
    EXPECT_EQ(raft->getCurrentTerm(), 0);
    EXPECT_EQ(raft->getSelfId(), selfId);
}

// Test for edge cases and error conditions
TEST_F(RaftImplTest, AppendEntriesWithConflictingEntries) {
    auto raft = std::make_unique<RaftImpl<MockClient>>(
        peers, selfId, *serviceChannel, policy, clientFactory, persister);
    
    // First, add an entry
    auto u1 = generate_uuid_v7();
    LogEntry entry1{1, 1, std::make_unique<zdb::Get>(u1, zdb::Key{"command1"})};
    raft->log().append(entry1);
    
    // Now try to append conflicting entry at same index with different term
    Log conflictingLog;
    auto u2 = generate_uuid_v7();
    LogEntry conflictingEntry{1, 2, std::make_unique<zdb::Set>(u2, zdb::Key{"conflicting-command"}, zdb::Value{"conflicting-value"})};
    conflictingLog.append(conflictingEntry);
    
    // This should cause the log to be truncated and the new entry added
    AppendEntriesArg arg{"leader1", 2, 0, 0, 1, conflictingLog};
    AppendEntriesReply reply = raft->appendEntriesHandler(arg);
    
    EXPECT_TRUE(reply.success);
    EXPECT_EQ(raft->log().lastIndex(), 1);
    
    // Verify the conflicting entry replaced the original
    auto retrievedEntry = raft->log().at(1);
    EXPECT_TRUE(retrievedEntry.has_value());
    EXPECT_EQ(retrievedEntry->term, 2);
    // Command comparison would need to be updated based on actual implementation
}

// Test for empty peers list
TEST_F(RaftImplTest, EmptyPeersList) {
    std::vector<std::string> emptyPeers = {};
    
    auto raft = std::make_unique<RaftImpl<MockClient>>(
        emptyPeers, selfId, *serviceChannel, policy, clientFactory, persister);

    EXPECT_EQ(raft->getRole(), Role::Follower);
    EXPECT_EQ(raft->getSelfId(), selfId);
    
    // Single node should be able to start commands immediately as leader
    // (This behavior depends on implementation - test actual behavior)
}

// Test that previously applied entries are not re-sent when new entries are added
TEST_F(RaftImplTest, DoesNotResendPreviouslyAppliedEntries) {
    auto raft = std::make_unique<RaftImpl<MockClient>>(
        peers, selfId, *serviceChannel, policy, clientFactory, persister);
    
    // First append - should apply entry 1
    Log firstBatch;
    auto u1 = generate_uuid_v7();
    LogEntry entry1{1, 1, std::make_unique<zdb::Get>(u1, zdb::Key{"command1"})};
    firstBatch.append(entry1);
    
    // Expect exactly one sendUntil call for the first entry
    EXPECT_CALL(*serviceChannel, sendUntil(_, _))
        .Times(1)
        .WillOnce(Return(true));
    
    AppendEntriesArg firstArg{"leader1", 1, 0, 0, 1, firstBatch};
    AppendEntriesReply firstReply = raft->appendEntriesHandler(firstArg);
    EXPECT_TRUE(firstReply.success);
    
    // Verify that entry was applied (Mock::VerifyAndClearExpectations clears call history)
    Mock::VerifyAndClearExpectations(serviceChannel.get());
    
    // Second append - should only apply entry 2, NOT re-apply entry 1
    Log secondBatch;
    auto u2 = generate_uuid_v7();
    LogEntry entry2{2, 1, std::make_unique<zdb::Set>(u2, zdb::Key{"command2"}, zdb::Value{"value2"})};
    secondBatch.append(entry2);
    
    // Critical test: expect exactly ONE sendUntil call for only the new entry
    // If the implementation incorrectly re-applies entry 1, this test will fail
    EXPECT_CALL(*serviceChannel, sendUntil(_, _))
        .Times(1)  // Should be exactly 1, not 2
        .WillOnce(Return(true));
    
    // Send second batch with prevLogIndex=1 (building on first entry)
    AppendEntriesArg secondArg{"leader1", 1, 1, 1, 2, secondBatch};
    AppendEntriesReply secondReply = raft->appendEntriesHandler(secondArg);
    EXPECT_TRUE(secondReply.success);
    
    // Verify log state
    EXPECT_EQ(raft->log().lastIndex(), 2);
    
    Mock::VerifyAndClearExpectations(serviceChannel.get());
    
    // Third append - add multiple entries but should only apply new ones
    Log thirdBatch;
    auto u3 = generate_uuid_v7();
    auto u4 = generate_uuid_v7();
    LogEntry entry3{3, 2, std::make_unique<zdb::Erase>(u3, zdb::Key{"command3"})};
    LogEntry entry4{4, 2, std::make_unique<zdb::Size>(u4)};
    thirdBatch.append(entry3);
    thirdBatch.append(entry4);
    
    // Should apply exactly 2 new entries (3 and 4), not re-apply 1 and 2
    EXPECT_CALL(*serviceChannel, sendUntil(_, _))
        .Times(2)  // Exactly 2 calls for the 2 new entries
        .WillRepeatedly(Return(true));
    
    AppendEntriesArg thirdArg{"leader1", 2, 2, 1, 4, thirdBatch};
    AppendEntriesReply thirdReply = raft->appendEntriesHandler(thirdArg);
    EXPECT_TRUE(thirdReply.success);
    
    EXPECT_EQ(raft->log().lastIndex(), 4);
}

// Test edge case: commit index moves but no new entries to apply
TEST_F(RaftImplTest, CommitIndexAdvanceWithoutNewEntries) {
    auto raft = std::make_unique<RaftImpl<MockClient>>(
        peers, selfId, *serviceChannel, policy, clientFactory, persister);
    
    // Add entries but don't commit them initially
    Log entriesLog;
    auto u1 = generate_uuid_v7();
    auto u2 = generate_uuid_v7();
    LogEntry entry1{1, 1, std::make_unique<zdb::Get>(u1, zdb::Key{"command1"})};
    LogEntry entry2{2, 1, std::make_unique<zdb::Set>(u2, zdb::Key{"command2"}, zdb::Value{"value2"})};
    entriesLog.append(entry1);
    entriesLog.append(entry2);
    
    // First append with leaderCommit=0 (no commits yet)
    EXPECT_CALL(*serviceChannel, sendUntil(_, _))
        .Times(0);  // No entries should be applied yet
    
    AppendEntriesArg firstArg{"leader1", 1, 0, 0, 0, entriesLog};
    AppendEntriesReply firstReply = raft->appendEntriesHandler(firstArg);
    EXPECT_TRUE(firstReply.success);
    
    Mock::VerifyAndClearExpectations(serviceChannel.get());
    
    // Second append with same entries but leaderCommit=2 (commit both)
    EXPECT_CALL(*serviceChannel, sendUntil(_, _))
        .Times(2)  // Both entries should be applied now
        .WillRepeatedly(Return(true));
    
    AppendEntriesArg secondArg{"leader1", 1, 0, 0, 2, entriesLog};
    AppendEntriesReply secondReply = raft->appendEntriesHandler(secondArg);
    EXPECT_TRUE(secondReply.success);
    
    Mock::VerifyAndClearExpectations(serviceChannel.get());
    
    // Third append with same commit index - should not re-apply anything
    EXPECT_CALL(*serviceChannel, sendUntil(_, _))
        .Times(0);  // No additional applications
    
    AppendEntriesArg thirdArg{"leader1", 1, 0, 0, 2, entriesLog};
    AppendEntriesReply thirdReply = raft->appendEntriesHandler(thirdArg);
    EXPECT_TRUE(thirdReply.success);
}

// Performance and stress tests
TEST_F(RaftImplTest, MultipleVoteRequests) {
    auto raft = std::make_unique<RaftImpl<MockClient>>(
        peers, selfId, *serviceChannel, policy, clientFactory, persister);
    
    const int numRequests = 100;
    std::vector<RequestVoteReply> replies;
    replies.reserve(numRequests);
    
    for (int i = 0; i < numRequests; ++i) {
        proto::RequestVoteArg protoArg;
        protoArg.set_candidateid("candidate" + std::to_string(i % 10));
        protoArg.set_term(static_cast<uint64_t>(i + 1));
        protoArg.set_lastlogindex(0);
        protoArg.set_lastlogterm(0);
        
        RequestVoteArg arg{protoArg};
        replies.push_back(raft->requestVoteHandler(arg));
    }
    
    // Verify all requests were handled
    EXPECT_EQ(replies.size(), numRequests);
    
    // Last reply should have the highest term
    EXPECT_EQ(replies.back().term, numRequests);
}
