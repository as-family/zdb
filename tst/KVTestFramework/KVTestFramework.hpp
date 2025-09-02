#ifndef KV_TEST_FRAMEWORK_H
#define KV_TEST_FRAMEWORK_H

#include "common/Error.hpp"
#include "common/Types.hpp"
#include "ProxyKVStoreService.hpp"
#include <string>
#include "server/KVStoreServiceImpl.hpp"
#include "client/KVStoreClient.hpp"
#include "client/Config.hpp"
#include <vector>
#include <functional>
#include <atomic>
#include "common/Types.hpp"
#include <expected>
#include "Porcupine.hpp"
#include "raft/Raft.hpp"
#include "raft/Channel.hpp"
#include "raft/TestRaft.hpp"
#include "storage/InMemoryKVStore.hpp"
#include "common/KVStateMachine.hpp"
#include "common/RetryPolicy.hpp"
#include <random>
#include <variant>
#include "server/RPCServer.hpp"

class KVTestFramework {
public:
    struct ClientResult {
        int nOK;
        int nMaybe;
    };
    Porcupine porcupine;
    KVTestFramework(const std::string& a, const std::string& t, NetworkConfig& c, raft::Channel& l, raft::Channel& f, raft::Raft& r, zdb::RetryPolicy p);
    std::vector<ClientResult> spawnClientsAndWait(
        int nClients,
        std::chrono::seconds timeout,
        std::vector<std::string> addresses,
        zdb::RetryPolicy policy,
        std::function<ClientResult(int id, zdb::KVStoreClient& client, std::atomic<bool>& done)> f
    );
    ClientResult oneClientSet(
        int clientId,
        zdb::KVStoreClient& client,
        std::vector<zdb::Key> keys,
        bool randomKeys,
        std::atomic<bool>& done
    );
    std::pair<int, bool> oneSet(
        int clientId,
        zdb::KVStoreClient& client,
        zdb::Key key,
        uint64_t version
    );
    std::expected<std::monostate, zdb::Error> setJson(int clientId, zdb::KVStoreClient& client, zdb::Key key, zdb::Value value);
    zdb::Value getJson(int clientId, zdb::KVStoreClient& client, zdb::Key key);
    bool checkSetConcurrent(
        zdb::KVStoreClient& client,
        zdb::Key key,
        std::vector<ClientResult> results
    );
    ~KVTestFramework();
private:
    std::string addr;
    std::string targetServerAddr;
    NetworkConfig& networkConfig;
    zdb::InMemoryKVStore mem;
    raft::Channel& leader;
    raft::Channel& follower;
    raft::Raft& raft;
    zdb::KVStateMachine kvState;
    zdb::KVStoreServiceImpl targetService;
    ProxyService<zdb::kvStore::KVStoreService> targetProxyService;
    ProxyKVStoreService service;
    zdb::RPCServer<zdb::KVStoreServiceImpl> targetServer;
    zdb::RPCServer<ProxyKVStoreService> server;
    std::default_random_engine rng;
};

#endif // KV_TEST_FRAMEWORK_H
