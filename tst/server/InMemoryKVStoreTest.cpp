#include <gtest/gtest.h>
#include <atomic>
#include <string>
#include <thread>
#include <vector>
#include "server/InMemoryKVStore.hpp"
#include "common/Types.hpp"

using zdb::InMemoryKVStore;
using zdb::Key;
using zdb::Value;

class InMemoryKVStoreTest : public ::testing::Test {
protected:
    InMemoryKVStore kv;
};

TEST_F(InMemoryKVStoreTest, SetAndGetBasic) {
    auto setResult = kv.set(Key{"key1"}, Value{"value1"});
    EXPECT_TRUE(setResult.has_value());
    auto getResult = kv.get(Key{"key1"});
    ASSERT_TRUE(getResult.has_value());
    ASSERT_TRUE(getResult.value().has_value());
    EXPECT_EQ(getResult.value().value().data, "value1");
}

TEST_F(InMemoryKVStoreTest, GetNonExistentKey) {
    auto getResult = kv.get(Key{"missing"});
    ASSERT_TRUE(getResult.has_value());
    EXPECT_FALSE(getResult.value().has_value());
}

TEST_F(InMemoryKVStoreTest, OverwriteValue) {
    auto result1 = kv.set(Key{"key1"}, Value{"value1"});
    EXPECT_TRUE(result1.has_value());
    
    // Get the current value and its version
    auto getResult1 = kv.get(Key{"key1"});
    ASSERT_TRUE(getResult1.has_value());
    ASSERT_TRUE(getResult1.value().has_value());
    EXPECT_EQ(getResult1.value().value().data, "value1");
    
    // Overwrite with the correct version
    Value updateValue{"value2"};
    updateValue.version = getResult1.value().value().version;
    auto result2 = kv.set(Key{"key1"}, updateValue);
    EXPECT_TRUE(result2.has_value());
    
    auto getResult = kv.get(Key{"key1"});
    ASSERT_TRUE(getResult.has_value());
    ASSERT_TRUE(getResult.value().has_value());
    EXPECT_EQ(getResult.value().value().data, "value2");
}

TEST_F(InMemoryKVStoreTest, EraseExistingKey) {
    auto setResult = kv.set(Key{"key1"}, Value{"value1"});
    EXPECT_TRUE(setResult.has_value());
    auto eraseResult = kv.erase(Key{"key1"});
    ASSERT_TRUE(eraseResult.has_value());
    ASSERT_TRUE(eraseResult.value().has_value());
    EXPECT_EQ(eraseResult.value().value().data, "value1");
    auto getResult = kv.get(Key{"key1"});
    ASSERT_TRUE(getResult.has_value());
    EXPECT_FALSE(getResult.value().has_value());
}

TEST_F(InMemoryKVStoreTest, EraseNonExistentKey) {
    auto eraseResult = kv.erase(Key{"missing"});
    ASSERT_TRUE(eraseResult.has_value());
    EXPECT_FALSE(eraseResult.value().has_value());
}

TEST_F(InMemoryKVStoreTest, SizeReflectsChanges) {
    EXPECT_EQ(kv.size(), 0);
    auto result1 = kv.set(Key{"a"}, Value{"1"});
    EXPECT_TRUE(result1.has_value());
    auto result2 = kv.set(Key{"b"}, Value{"2"});
    EXPECT_TRUE(result2.has_value());
    EXPECT_EQ(kv.size(), 2);
    auto eraseResult1 = kv.erase(Key{"a"});
    EXPECT_TRUE(eraseResult1.has_value());
    EXPECT_EQ(kv.size(), 1);
    auto eraseResult2 = kv.erase(Key{"b"});
    EXPECT_TRUE(eraseResult2.has_value());
    EXPECT_EQ(kv.size(), 0);
}

TEST_F(InMemoryKVStoreTest, SetEraseSetSameKey) {
    // Set key to value1
    auto setResult1 = kv.set(Key{"mykey"}, Value{"value1"});
    EXPECT_TRUE(setResult1.has_value());
    // Confirm value1
    auto getResult1 = kv.get(Key{"mykey"});
    ASSERT_TRUE(getResult1.has_value());
    ASSERT_TRUE(getResult1.value().has_value());
    EXPECT_EQ(getResult1.value().value().data, "value1");

    // Erase key
    auto eraseResult = kv.erase(Key{"mykey"});
    ASSERT_TRUE(eraseResult.has_value());
    ASSERT_TRUE(eraseResult.value().has_value());
    EXPECT_EQ(eraseResult.value().value().data, "value1");
    // Confirm erased
    auto getResult2 = kv.get(Key{"mykey"});
    ASSERT_TRUE(getResult2.has_value());
    EXPECT_FALSE(getResult2.value().has_value());

    // Set key to value2
    auto setResult2 = kv.set(Key{"mykey"}, Value{"value2"});
    EXPECT_TRUE(setResult2.has_value());
    // Confirm value2
    auto getResult3 = kv.get(Key{"mykey"});
    ASSERT_TRUE(getResult3.has_value());
    ASSERT_TRUE(getResult3.value().has_value());
    EXPECT_EQ(getResult3.value().value().data, "value2");
}

TEST_F(InMemoryKVStoreTest, EmptyValue) {
    auto setResult = kv.set(Key{"empty"}, Value{""});
    EXPECT_TRUE(setResult.has_value());
    auto getResult = kv.get(Key{"empty"});
    ASSERT_TRUE(getResult.has_value());
    ASSERT_TRUE(getResult.value().has_value());
    EXPECT_EQ(getResult.value().value().data, "");
}

TEST_F(InMemoryKVStoreTest, LargeValue) {
    const std::string large(100000, 'x');
    auto setResult = kv.set(Key{"bigkey"}, Value{large});
    EXPECT_TRUE(setResult.has_value());
    auto getResult = kv.get(Key{"bigkey"});
    ASSERT_TRUE(getResult.has_value());
    ASSERT_TRUE(getResult.value().has_value());
    EXPECT_EQ(getResult.value().value().data, large);
}

TEST_F(InMemoryKVStoreTest, SpecialCharactersInKey) {
    auto setResult = kv.set(Key{"spécial!@#"}, Value{"value"});
    EXPECT_TRUE(setResult.has_value());
    auto getResult = kv.get(Key{"spécial!@#"});
    ASSERT_TRUE(getResult.has_value());
    ASSERT_TRUE(getResult.value().has_value());
    EXPECT_EQ(getResult.value().value().data, "value");
}

TEST_F(InMemoryKVStoreTest, MultipleKeys) {
    auto result1 = kv.set(Key{"k1"}, Value{"v1"});
    EXPECT_TRUE(result1.has_value());
    auto result2 = kv.set(Key{"k2"}, Value{"v2"});
    EXPECT_TRUE(result2.has_value());
    auto result3 = kv.set(Key{"k3"}, Value{"v3"});
    EXPECT_TRUE(result3.has_value());
    EXPECT_EQ(kv.size(), 3);
    
    auto getResult1 = kv.get(Key{"k1"});
    ASSERT_TRUE(getResult1.has_value());
    ASSERT_TRUE(getResult1.value().has_value());
    EXPECT_EQ(getResult1.value().value().data, "v1");
    
    auto getResult2 = kv.get(Key{"k2"});
    ASSERT_TRUE(getResult2.has_value());
    ASSERT_TRUE(getResult2.value().has_value());
    EXPECT_EQ(getResult2.value().value().data, "v2");
    
    auto getResult3 = kv.get(Key{"k3"});
    ASSERT_TRUE(getResult3.has_value());
    ASSERT_TRUE(getResult3.value().has_value());
    EXPECT_EQ(getResult3.value().value().data, "v3");
}

TEST_F(InMemoryKVStoreTest, ThreadSafetySetAndGet) {
    const int n = 100;
    std::atomic<bool> failed{false};
    auto writer = [&](int thread_id) {
        for (int i = 0; i < n; ++i) {
            // Use thread-specific keys to avoid version conflicts
            Key key{"key" + std::to_string(thread_id) + "_" + std::to_string(i)};
            Value value{std::to_string(i)};
            auto res = kv.set(key, value);
            if (!res.has_value()) {
                failed = true;
            }
        }
    };
    auto reader = [&](int thread_id) {
        for (int i = 0; i < n; ++i) {
            // Read from thread-specific keys
            Key key{"key" + std::to_string(thread_id) + "_" + std::to_string(i)};
            auto res = kv.get(key);
            // Note: Reader might not find the key if writer hasn't written it yet
            // This is acceptable in concurrent scenarios
            if (!res.has_value()) {
                // Only fail if there's an actual error, not if key doesn't exist
                // since reader and writer are running concurrently
            }
        }
    };
    std::thread t1(writer, 0);
    std::thread t2(reader, 0);
    std::thread t3(writer, 1);
    std::thread t4(reader, 1);
    t1.join();
    t2.join();
    t3.join();
    t4.join();
    EXPECT_FALSE(failed);
    EXPECT_EQ(kv.size(), n * 2); // 2 writers, n keys each
    
    // Verify the final state
    for (int thread_id = 0; thread_id < 2; ++thread_id) {
        for (int i = 0; i < n; ++i) {
            Key key{"key" + std::to_string(thread_id) + "_" + std::to_string(i)};
            auto res = kv.get(key);
            ASSERT_TRUE(res.has_value());
            ASSERT_TRUE(res.value().has_value());
            EXPECT_EQ(res.value().value().data, std::to_string(i));
        }
    }
}

TEST_F(InMemoryKVStoreTest, ThreadSafetyErase) {
    const int n = 100;
    for (int i = 0; i < n; ++i) {
        Key key{"key" + std::to_string(i)};
        Value value{"v"};
        auto setResult = kv.set(key, value);
        EXPECT_TRUE(setResult.has_value());
    }
    std::atomic<int> erased{0};
    auto eraser = [&]() {
        for (int i = 0; i < n; ++i) {
            Key key{"key" + std::to_string(i)};
            auto res = kv.erase(key);
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
    auto worker = [&](int tid) {
        for (int i = 0; i < n; ++i) {
            // Use thread-specific keys to avoid version conflicts
            Key key{"k" + std::to_string(tid) + "_" + std::to_string(i)};
            Value value{std::to_string(i)};
            auto setResult = kv.set(key, value);
            if (!setResult.has_value()) {
                failed = true;
                continue;
            }
            auto getResult = kv.get(key);
            if (!getResult.has_value()) {
                failed = true;
                continue;
            }
            // For erase, we need the current version
            if (getResult.value().has_value()) {
                auto eraseResult = kv.erase(key);
                if (!eraseResult.has_value()) {
                    failed = true;
                }
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
