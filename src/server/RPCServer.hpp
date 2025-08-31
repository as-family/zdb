#ifndef RPC_SERVER_H
#define RPC_SERVER_H

#include <memory>
#include <grpcpp/grpcpp.h>
#include <thread>
#include <stdexcept>

namespace zdb {

template<typename Service>
class RPCServer {
public:
    RPCServer(const std::string& address, Service& s);
    ~RPCServer();
    RPCServer(const RPCServer&) = delete;
    RPCServer& operator=(const RPCServer&) = delete;
    RPCServer(RPCServer&&) = delete;
    RPCServer& operator=(RPCServer&&) = delete;
    void wait();
    void shutdown();
private:
    std::string addr;
    Service& service;
    std::unique_ptr<grpc::Server> server;
};

template<typename Service>
RPCServer<Service>::RPCServer(const std::string& address, Service& s)
    : addr{address}, service {s} {
    grpc::ServerBuilder sb{};
    sb.AddListeningPort(addr, grpc::InsecureServerCredentials());
    sb.RegisterService(&service);
    server = sb.BuildAndStart();
    if (!server) {
        throw std::runtime_error("Failed to start gRPC server on address: " + address);
    }
}

template<typename Service>
void RPCServer<Service>::wait() {
    if (server) {
        server->Wait();
    }
}

template<typename Service>
void RPCServer<Service>::shutdown() {
    if (server) {
        server->Shutdown();
        server.reset();
    }
}

template<typename Service>
RPCServer<Service>::~RPCServer() {
    shutdown();
}

} // namespace zdb

#endif // RPC_SERVER_H
