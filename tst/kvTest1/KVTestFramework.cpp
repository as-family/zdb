#include "KVTestFramework.hpp"
#include <string>
#include <grpcpp/grpcpp.h>
#include "server/InMemoryKVStore.hpp"
#include "client/Config.hpp"
#include "client/KVStoreClient.hpp"

KVTestFramework::KVTestFramework(std::string a, std::string t, NetworkConfig& c)
    : addr {a},
      targetServerAddr {t},
      networkConfig(c),
      service {ProxyKVStoreService {targetServerAddr, networkConfig}},
      mem {zdb::InMemoryKVStore {}},
      targetService {zdb::KVStoreServiceImpl {mem}} {
    grpc::ServerBuilder targetSB{};
    targetSB.AddListeningPort(targetServerAddr, grpc::InsecureServerCredentials());
    targetSB.RegisterService(&targetService);
    targetServer = targetSB.BuildAndStart();
    targetServerThread = std::thread([this]() { targetServer->Wait(); });

    grpc::ServerBuilder sb{};
    sb.AddListeningPort(addr, grpc::InsecureServerCredentials());
    sb.RegisterService(&service);
    server = sb.BuildAndStart();
    serverThread = std::thread([this]() { server->Wait(); });
}

zdb::KVStoreClient KVTestFramework::makeClient(zdb::Config& config) {
    return zdb::KVStoreClient {config};
}

KVTestFramework::~KVTestFramework() {
    server->Shutdown();
    serverThread.join();
    targetServer->Shutdown();
    targetServerThread.join();
}
