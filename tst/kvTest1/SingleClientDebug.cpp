#include "KVTestFramework.hpp"
#include <gtest/gtest.h>

class SingleClientTest : public ::testing::Test {
protected:
    void SetUp() override {
        ts = std::make_unique<KVTestFramework>(true); // reliable
    }
    
    void TearDown() override {
        ts.reset();
    }
    
    std::unique_ptr<KVTestFramework> ts;
};

TEST_F(SingleClientTest, SingleClientErrorTest) {
    ts->Begin("Single client error test");
    
    // Create just one client
    auto client = ts->makeClient();
    
    // Test the exact same operation that's failing in the main test
    std::cout << "Testing Put with version 1 on non-existent key 'k'...\n";
    auto err = ts->PutJson(*client, "k", "", TVersion(1), 0);
    std::cout << "Error result: " << KVTestFramework::ErrorToString(err) << " (" << static_cast<int>(err) << ")\n";
    
    // This should be ErrNoKey
    EXPECT_EQ(err, KVError::ErrNoKey);
    
    // Test with version 0 (should succeed)
    std::cout << "Testing Put with version 0 on non-existent key 'k2'...\n";
    auto err2 = ts->PutJson(*client, "k2", "value", TVersion(0), 0);
    std::cout << "Error result: " << KVTestFramework::ErrorToString(err2) << " (" << static_cast<int>(err2) << ")\n";
    
    // This should succeed
    EXPECT_EQ(err2, KVError::OK);
}
