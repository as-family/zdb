#ifndef RPC_SERVER_H
#define RPC_SERVER_H

#include <memory>
#include <grpcpp/grpcpp.h>
#include <thread>
#include <stdexcept>
#include <chrono>

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
    std::thread serverThread;
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
    serverThread = std::thread([this]() { this->wait(); });
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
        auto deadline = std::chrono::system_clock::now() + std::chrono::milliseconds(10);
        server->Shutdown(deadline);
    }
    if (serverThread.joinable()) {
        serverThread.join();
    }
    server.reset();
}

template<typename Service>
RPCServer<Service>::~RPCServer() {
    shutdown();
}

} // namespace zdb

#endif // RPC_SERVER_H
