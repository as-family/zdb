#ifndef PEER_NODE_H
#define PEER_NODE_H

#include <memory>
#include <string>
#include <vector>
#include <map>
#include <grpcpp/grpcpp.h>
#include "src/proto/peer.grpc.pb.h"

namespace zdb {

class PeerNode {
private:
    class PeerServiceImpl final : public peer::PeerService::Service {
    private:
        PeerNode* node;

    public:
        PeerServiceImpl(PeerNode* node);

        grpc::Status Ping(grpc::ServerContext* context,
                         const peer::PingRequest* request,
                         peer::PingReply* reply) override;
    };

public:
    PeerNode(const std::string& peer_id, const std::string& listen_address,
             const std::vector<std::string>& peer_addresses);

    void startServer();
    void pingPong();
private:
    std::string peer_id;
    std::string listen_address;
    std::unique_ptr<grpc::Server> server;
    std::vector<std::string> peer_addresses;
    std::unique_ptr<PeerServiceImpl> service;
    std::map<std::string, std::unique_ptr<peer::PeerService::Stub>> stubs;
};

} // namespace zdb

#endif // PEER_NODE_H
