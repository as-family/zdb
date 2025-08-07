#include <gtest/gtest.h>
#include <atomic>
#include <string>
#include <thread>
#include <vector>
#include "server/InMemoryKVStore.hpp"

using zdb::InMemoryKVStore;

class InMemoryKVStoreTest : public ::testing::Test {
protected:
    InMemoryKVStore kv;
};

TEST_F(InMemoryKVStoreTest, SetAndGetBasic) {
    auto setResult = kv.set("key1", "value1");
    EXPECT_TRUE(setResult.has_value());
    auto getResult = kv.get("key1");
    ASSERT_TRUE(getResult.has_value());
    ASSERT_TRUE(getResult.value().has_value());
    EXPECT_EQ(getResult.value().value(), "value1");
}

TEST_F(InMemoryKVStoreTest, GetNonExistentKey) {
    auto getResult = kv.get("missing");
    ASSERT_TRUE(getResult.has_value());
    EXPECT_FALSE(getResult.value().has_value());
}

TEST_F(InMemoryKVStoreTest, OverwriteValue) {
    auto result1 = kv.set("key1", "value1");
    EXPECT_TRUE(result1.has_value());
    auto result2 = kv.set("key1", "value2");
    EXPECT_TRUE(result2.has_value());
    auto getResult = kv.get("key1");
    ASSERT_TRUE(getResult.has_value());
    ASSERT_TRUE(getResult.value().has_value());
    EXPECT_EQ(getResult.value().value(), "value2");
}

TEST_F(InMemoryKVStoreTest, EraseExistingKey) {
    auto setResult = kv.set("key1", "value1");
    EXPECT_TRUE(setResult.has_value());
    auto eraseResult = kv.erase("key1");
    ASSERT_TRUE(eraseResult.has_value());
    ASSERT_TRUE(eraseResult.value().has_value());
    EXPECT_EQ(eraseResult.value().value(), "value1");
    auto getResult = kv.get("key1");
    ASSERT_TRUE(getResult.has_value());
    EXPECT_FALSE(getResult.value().has_value());
}

TEST_F(InMemoryKVStoreTest, EraseNonExistentKey) {
    auto eraseResult = kv.erase("missing");
    ASSERT_TRUE(eraseResult.has_value());
    EXPECT_FALSE(eraseResult.value().has_value());
}

TEST_F(InMemoryKVStoreTest, SizeReflectsChanges) {
    EXPECT_EQ(kv.size(), 0);
    auto result1 = kv.set("a", "1");
    EXPECT_TRUE(result1.has_value());
    auto result2 = kv.set("b", "2");
    EXPECT_TRUE(result2.has_value());
    EXPECT_EQ(kv.size(), 2);
    auto eraseResult1 = kv.erase("a");
    EXPECT_TRUE(eraseResult1.has_value());
    EXPECT_EQ(kv.size(), 1);
    auto eraseResult2 = kv.erase("b");
    EXPECT_TRUE(eraseResult2.has_value());
    EXPECT_EQ(kv.size(), 0);
}

TEST_F(InMemoryKVStoreTest, EmptyValue) {
    auto setResult = kv.set("empty", "");
    EXPECT_TRUE(setResult.has_value());
    auto getResult = kv.get("empty");
    ASSERT_TRUE(getResult.has_value());
    ASSERT_TRUE(getResult.value().has_value());
    EXPECT_EQ(getResult.value().value(), "");
}

TEST_F(InMemoryKVStoreTest, LargeValue) {
    const std::string large(100000, 'x');
    auto setResult = kv.set("bigkey", large);
    EXPECT_TRUE(setResult.has_value());
    auto getResult = kv.get("bigkey");
    ASSERT_TRUE(getResult.has_value());
    ASSERT_TRUE(getResult.value().has_value());
    EXPECT_EQ(getResult.value().value(), large);
}

TEST_F(InMemoryKVStoreTest, SpecialCharactersInKey) {
    auto setResult = kv.set("spécial!@#", "value");
    EXPECT_TRUE(setResult.has_value());
    auto getResult = kv.get("spécial!@#");
    ASSERT_TRUE(getResult.has_value());
    ASSERT_TRUE(getResult.value().has_value());
    EXPECT_EQ(getResult.value().value(), "value");
}

TEST_F(InMemoryKVStoreTest, MultipleKeys) {
    auto result1 = kv.set("k1", "v1");
    EXPECT_TRUE(result1.has_value());
    auto result2 = kv.set("k2", "v2");
    EXPECT_TRUE(result2.has_value());
    auto result3 = kv.set("k3", "v3");
    EXPECT_TRUE(result3.has_value());
    EXPECT_EQ(kv.size(), 3);
    
    auto getResult1 = kv.get("k1");
    ASSERT_TRUE(getResult1.has_value());
    ASSERT_TRUE(getResult1.value().has_value());
    EXPECT_EQ(getResult1.value().value(), "v1");
    
    auto getResult2 = kv.get("k2");
    ASSERT_TRUE(getResult2.has_value());
    ASSERT_TRUE(getResult2.value().has_value());
    EXPECT_EQ(getResult2.value().value(), "v2");
    
    auto getResult3 = kv.get("k3");
    ASSERT_TRUE(getResult3.has_value());
    ASSERT_TRUE(getResult3.value().has_value());
    EXPECT_EQ(getResult3.value().value(), "v3");
}

TEST_F(InMemoryKVStoreTest, ThreadSafetySetAndGet) {
    const int n = 100;
    std::atomic<bool> failed{false};
    auto writer = [&]() {
        for (int i = 0; i < n; ++i) {
            auto res = kv.set("key" + std::to_string(i), std::to_string(i));
            if (!res.has_value()) {
                failed = true;
            }
        }
    };
    auto reader = [&]() {
        for (int i = 0; i < n; ++i) {
            auto res = kv.get("key" + std::to_string(i));
            if (!res.has_value()) {
                failed = true;
            }
        }
    };
    std::thread t1(writer);
    std::thread t2(reader);
    std::thread t3(writer);
    std::thread t4(reader);
    t1.join();
    t2.join();
    t3.join();
    t4.join();
    EXPECT_FALSE(failed);
    EXPECT_EQ(kv.size(), n);
    for (int i = 0; i < n; ++i) {
        auto res = kv.get("key" + std::to_string(i));
        ASSERT_TRUE(res.has_value());
        ASSERT_TRUE(res.value().has_value());
        EXPECT_EQ(res.value().value(), std::to_string(i));
    }
}

TEST_F(InMemoryKVStoreTest, ThreadSafetyErase) {
    const int n = 100;
    for (int i = 0; i < n; ++i) {
        auto setResult = kv.set("key" + std::to_string(i), "v");
        EXPECT_TRUE(setResult.has_value());
    }
    std::atomic<int> erased{0};
    auto eraser = [&]() {
        for (int i = 0; i < n; ++i) {
            auto res = kv.erase("key" + std::to_string(i));
            if (res.has_value() && res.value().has_value()) {
                ++erased;
            }
        }
    };
    std::thread t1(eraser);
    std::thread t2(eraser);
    t1.join();
    t2.join();
    EXPECT_EQ(erased, n);
    EXPECT_EQ(kv.size(), 0);
}

TEST_F(InMemoryKVStoreTest, StressTestConcurrentSetGetErase) {
    const int n = 1000;
    std::atomic<bool> failed{false};
    auto worker = [&](int /*tid*/) {
        for (int i = 0; i < n; ++i) {
            const std::string key = "k" + std::to_string(i);
            auto setResult = kv.set(key, std::to_string(i));
            if (!setResult.has_value()) {
                failed = true;
            }
            auto getResult = kv.get(key);
            if (!getResult.has_value()) {
                failed = true;
            }
            auto eraseResult = kv.erase(key);
            if (!eraseResult.has_value()) {
                failed = true;
            }
        }
    };
    std::vector<std::thread> threads;
    threads.reserve(8);
    for (int i = 0; i < 8; ++i) {
        threads.emplace_back(worker, i);
    }
    for (auto& t : threads) {
        t.join();
    }
    EXPECT_FALSE(failed);
    EXPECT_EQ(kv.size(), 0);
}

// Error path tests: Simulate error by using invalid keys/values if possible.
// If Error is only used for future extension, these tests will pass as normal.
// If you add error injection to InMemoryKVStore, add tests here.
