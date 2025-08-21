#include <gtest/gtest.h>
#include "raft/Raft.hpp"
#include "raft/RaftImpl.hpp"

TEST(Raft, Init) {
    raft::Raft* r = new raft::RaftImpl();
    delete r;
}


