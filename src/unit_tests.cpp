#include <gtest/gtest.h>
#include "InMemoryKVStore.hpp"

using namespace zdb;

class InMemoryKVStoreTest : public testing::Test {
protected:
    InMemoryKVStore kv{};
};

TEST_F(InMemoryKVStoreTest, StartsEmpty) {
    ASSERT_EQ(kv.size(), 0);
}
