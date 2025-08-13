#ifndef KV_TEST_FRAMEWORK_H
#define KV_TEST_FRAMEWORK_H

#include "common/Error.hpp"
#include "common/Types.hpp"
#include "ProxyKVStoreService.hpp"
#include <string>
#include "server/KVStoreServer.hpp"
#include "client/KVStoreClient.hpp"
#include "client/Config.hpp"

class KVTestFramework {
public:
    KVTestFramework(std::string a, std::string r, NetworkConfig& c);
    zdb::KVStoreClient makeClient(zdb::Config& config);
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
};

#endif // KV_TEST_FRAMEWORK_H
