#include "KVTestFramework.hpp"
#include <gtest/gtest.h>

class ErrorTestDebug : public ::testing::Test {
protected:
    std::unique_ptr<KVTestFramework> ts;
    
    void SetUp() override {
        ts = std::make_unique<KVTestFramework>(true);
    }
    
    void TearDown() override {
        if (ts) {
            ts->~KVTestFramework();
            ts.reset();
        }
    }
};

TEST_F(ErrorTestDebug, DebugVersionError) {
    ts->Begin("Debug version error handling");
    
    auto ck = ts->makeClient();
    
    auto result1 = ts->PutJson(*ck, "debug_key", "value1", 0, 1);
    EXPECT_EQ(result1, zdb::ErrorCode::OK);
    
    // Now try to put with wrong version (should get ErrVersion)
    auto result2 = ts->PutJson(*ck, "debug_key", "value2", 0, 1);
    EXPECT_EQ(result2, zdb::ErrorCode::VersionMismatch);
    
    // Now try to put to non-existent key with non-zero version (should get ErrNoKey)
    auto result3 = ts->PutJson(*ck, "nonexistent_debug", "value3", 5, 1);
    EXPECT_EQ(result3, zdb::ErrorCode::KeyNotFound);
}
