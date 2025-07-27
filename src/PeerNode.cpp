#include "PeerNode.hpp"
#include <thread>
#include <chrono>
#include <iostream>

using grpc::Channel;
using grpc::ClientContext;
using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::Status;

namespace zdb {
using peer::PeerService;
using peer::PingReply;
using peer::PingRequest;

PeerNode::PeerServiceImpl::PeerServiceImpl(PeerNode *n) : node(n) {}

Status PeerNode::PeerServiceImpl::Ping(ServerContext *context,
                                       const PingRequest *request,
                                       PingReply *reply) {
    std::cout << "[" << node->peer_id << "] Received: "
              << request->message() << " from "
              << request->from_peer()
              << std::endl;

    reply->set_from_peer(node->peer_id);
    reply->set_message("PONG");
    return Status::OK;
}

PeerNode::PeerNode(const std::string &p_id, const std::string &l_address,
                   const std::vector<std::string> &p_addresses)
    : peer_id(p_id), listen_address(l_address),
      peer_addresses(p_addresses), service(std::make_unique<PeerServiceImpl>(this)) {
        for (const auto &peer_addr : peer_addresses) {
            if (peer_addr == listen_address)
                continue; // Skip self

            auto channel = grpc::CreateChannel(peer_addr,
                                            grpc::InsecureChannelCredentials());
            auto stub = PeerService::NewStub(channel);
            stubs[peer_addr] = std::move(stub);
        }
      }

void PeerNode::startServer() {
    ServerBuilder builder;

    builder.AddListeningPort(listen_address,
                             grpc::InsecureServerCredentials());
    builder.RegisterService(service.get());

    server = builder.BuildAndStart();
    std::cout << "[" << peer_id << "] Server listening on "
              << listen_address << std::endl;
}

void PeerNode::pingPong() {
    std::cout << "[" << peer_id << "] Starting ping-pong..." << std::endl;

    while (true) {
        for (const auto& [peer_addr, stub] : stubs) {
            PingRequest request;
            request.set_from_peer(peer_id);
            request.set_message("PING");

            PingReply reply;
            ClientContext context;

            Status status = stub->Ping(&context, request, &reply);

            if (status.ok()) {
                std::cout << "[" << peer_id << "] Sent PING to "
                          << peer_addr << ", got " << reply.message()
                          << " from " << reply.from_peer() << std::endl;
            }
            else {
                std::cout << "[" << peer_id << "] Failed to ping "
                          << peer_addr << std::endl;
            }
        }

        std::this_thread::sleep_for(std::chrono::seconds(3));
    }
}

} // namespace zdb
