#include "PeerNode.hpp"
#include <thread>
#include <chrono>
#include <iostream>
#include <string>
#include <vector>
#include <memory>
#include <grpcpp/grpcpp.h>
#include <grpcpp/client_context.h>
#include <grpcpp/server.h>
#include <grpcpp/server_builder.h>
#include <grpcpp/server_context.h>
#include <grpcpp/create_channel.h>
#include <grpcpp/security/credentials.h>

using grpc::ClientContext;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::Status;

namespace zdb {
using peer::PeerService;
using peer::PingReply;
using peer::PingRequest;

PeerNode::PeerServiceImpl::PeerServiceImpl(PeerNode *n) : node(n) {}

Status PeerNode::PeerServiceImpl::Ping(ServerContext * /*context*/,
                                       const PingRequest *request,
                                       PingReply *reply) {
    std::cout << "[" << node->peer_id << "] Received: "
              << request->message() << " from "
              << request->from_peer()
              << '\n';

    reply->set_from_peer(node->peer_id);
    reply->set_message("PONG");
    return Status::OK;
}

PeerNode::PeerNode(const std::string &peerId, const std::string &listenAddress,
                   const std::vector<std::string> &peerAddresses)
    : peer_id(peerId), listen_address(listenAddress),
      peer_addresses(peerAddresses), service(std::make_unique<PeerServiceImpl>(this)) {
        for (const auto &peerAddr : peer_addresses) {
            if (peerAddr == listen_address) {
                continue; // Skip self
            }

            auto channel = grpc::CreateChannel(peerAddr,
                                            grpc::InsecureChannelCredentials());
            auto stub = PeerService::NewStub(channel);
            stubs[peerAddr] = std::move(stub);
        }
      }

void PeerNode::startServer() {
    ServerBuilder builder;

    builder.AddListeningPort(listen_address,
                             grpc::InsecureServerCredentials());
    builder.RegisterService(service.get());

    server = builder.BuildAndStart();
    std::cout << "[" << peer_id << "] Server listening on "
              << listen_address << '\n';
}

void PeerNode::pingPong() {
    std::cout << "[" << peer_id << "] Starting ping-pong..." << '\n';

    while (true) {
        for (const auto& [peerAddr, stub] : stubs) {
            PingRequest request;
            request.set_from_peer(peer_id);
            request.set_message("PING");

            PingReply reply;
            ClientContext context;

            const Status status = stub->Ping(&context, request, &reply);

            if (status.ok()) {
                std::cout << "[" << peer_id << "] Sent PING to "
                          << peerAddr << ", got " << reply.message()
                          << " from " << reply.from_peer() << '\n';
            }
            else {
                std::cout << "[" << peer_id << "] Failed to ping "
                          << peerAddr << '\n';
            }
        }

        std::this_thread::sleep_for(std::chrono::seconds(3));
    }
}

} // namespace zdb
