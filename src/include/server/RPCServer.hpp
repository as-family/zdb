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
    serverThread = std::thread([this]() { server->Wait(); });
}

template<typename Service>
void RPCServer<Service>::shutdown() {
    if (server) {
        auto deadline = std::chrono::system_clock::now() + std::chrono::milliseconds{10L};
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
