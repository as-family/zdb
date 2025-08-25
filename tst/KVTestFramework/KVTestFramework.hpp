#ifndef KV_TEST_FRAMEWORK_H
#define KV_TEST_FRAMEWORK_H

#include "common/Error.hpp"
#include "common/Types.hpp"
#include "ProxyKVStoreService.hpp"
#include <string>
#include "server/KVStoreServer.hpp"
#include "client/KVStoreClient.hpp"
#include "client/Config.hpp"
#include <vector>
#include <functional>
#include <atomic>
#include "common/Types.hpp"
#include <expected>
#include "Porcupine.hpp"
#include "common/RetryPolicy.hpp"
#include "raft/Raft.hpp"
#include "raft/Channel.hpp"

class KVTestFramework {
public:
    struct ClientResult {
        int nOK;
        int nMaybe;
    };
    Porcupine porcupine;
    KVTestFramework(std::string a, std::string t, NetworkConfig& c, raft::Raft* r = nullptr, raft::Channel* ch = nullptr);
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
    std::expected<void, zdb::Error> setJson(int clientId, zdb::KVStoreClient& client, zdb::Key key, zdb::Value value);
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
    ProxyKVStoreService service;
    zdb::InMemoryKVStore mem;
    zdb::KVStoreServiceImpl targetService;
    std::unique_ptr<grpc::Server> targetServer;
    std::unique_ptr<grpc::Server> server;
    std::thread serverThread;
    std::thread targetServerThread;
    std::default_random_engine rng;
};

#endif // KV_TEST_FRAMEWORK_H
