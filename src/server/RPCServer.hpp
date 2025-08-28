#ifndef RPC_SERVER_H
#define RPC_SERVER_H

#include <memory>
#include <grpcpp/grpcpp.h>
#include <thread>

namespace zdb {

template<typename Service>
class RPCServer {
public:
    RPCServer(const std::string& address, Service& s);
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
}

template<typename Service>
void RPCServer<Service>::wait() {
    server->Wait();
}

template<typename Service>
void RPCServer<Service>::shutdown() {
    if (server) {
        server->Shutdown();
        server.reset();
    }
}

} // namespace zdb

#endif // RPC_SERVER_H
