#include <gtest/gtest.h>
#include "raft/Raft.hpp"
#include "raft/RaftImpl.hpp"
#include "raft/Channel.hpp"
#include <string>
#include <vector>

TEST(Raft, Init) {
    raft::Channel c{};
    std::vector<std::string> v{};
    raft::Raft* r = new raft::RaftImpl(v, "", c);
    delete r;
}
