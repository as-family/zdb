#include <gtest/gtest.h>
#include <thread>
#include <vector>
#include <atomic>
#include "../InMemoryKVStore.hpp"

using namespace zdb;

class InMemoryKVStoreTest : public ::testing::Test {
protected:
    InMemoryKVStore kv;
};

TEST_F(InMemoryKVStoreTest, SetAndGetBasic) {
    auto set_result = kv.set("key1", "value1");
    EXPECT_TRUE(set_result.has_value());
    auto get_result = kv.get("key1");
    ASSERT_TRUE(get_result.has_value());
    ASSERT_TRUE(get_result.value().has_value());
    EXPECT_EQ(get_result.value().value(), "value1");
}

TEST_F(InMemoryKVStoreTest, GetNonExistentKey) {
    auto get_result = kv.get("missing");
    ASSERT_TRUE(get_result.has_value());
    EXPECT_FALSE(get_result.value().has_value());
}

TEST_F(InMemoryKVStoreTest, OverwriteValue) {
    kv.set("key1", "value1");
    kv.set("key1", "value2");
    auto get_result = kv.get("key1");
    ASSERT_TRUE(get_result.has_value());
    ASSERT_TRUE(get_result.value().has_value());
    EXPECT_EQ(get_result.value().value(), "value2");
}

TEST_F(InMemoryKVStoreTest, EraseExistingKey) {
    kv.set("key1", "value1");
    auto erase_result = kv.erase("key1");
    ASSERT_TRUE(erase_result.has_value());
    ASSERT_TRUE(erase_result.value().has_value());
    EXPECT_EQ(erase_result.value().value(), "value1");
    auto get_result = kv.get("key1");
    ASSERT_TRUE(get_result.has_value());
    EXPECT_FALSE(get_result.value().has_value());
}

TEST_F(InMemoryKVStoreTest, EraseNonExistentKey) {
    auto erase_result = kv.erase("missing");
    ASSERT_TRUE(erase_result.has_value());
    EXPECT_FALSE(erase_result.value().has_value());
}

TEST_F(InMemoryKVStoreTest, SizeReflectsChanges) {
    EXPECT_EQ(kv.size(), 0);
    kv.set("a", "1");
    kv.set("b", "2");
    EXPECT_EQ(kv.size(), 2);
    kv.erase("a");
    EXPECT_EQ(kv.size(), 1);
    kv.erase("b");
    EXPECT_EQ(kv.size(), 0);
}

TEST_F(InMemoryKVStoreTest, EmptyValue) {
    kv.set("empty", "");
    auto get_result = kv.get("empty");
    ASSERT_TRUE(get_result.has_value());
    ASSERT_TRUE(get_result.value().has_value());
    EXPECT_EQ(get_result.value().value(), "");
}

TEST_F(InMemoryKVStoreTest, LargeValue) {
    std::string large(100000, 'x');
    kv.set("bigkey", large);
    auto get_result = kv.get("bigkey");
    ASSERT_TRUE(get_result.has_value());
    ASSERT_TRUE(get_result.value().has_value());
    EXPECT_EQ(get_result.value().value(), large);
}

TEST_F(InMemoryKVStoreTest, SpecialCharactersInKey) {
    kv.set("spécial!@#", "value");
    auto get_result = kv.get("spécial!@#");
    ASSERT_TRUE(get_result.has_value());
    ASSERT_TRUE(get_result.value().has_value());
    EXPECT_EQ(get_result.value().value(), "value");
}

TEST_F(InMemoryKVStoreTest, MultipleKeys) {
    kv.set("k1", "v1");
    kv.set("k2", "v2");
    kv.set("k3", "v3");
    EXPECT_EQ(kv.size(), 3);
    EXPECT_EQ(kv.get("k1").value().value(), "v1");
    EXPECT_EQ(kv.get("k2").value().value(), "v2");
    EXPECT_EQ(kv.get("k3").value().value(), "v3");
}

TEST_F(InMemoryKVStoreTest, ThreadSafetySetAndGet) {
    const int N = 100;
    std::atomic<bool> failed{false};
    auto writer = [&](){
        for (int i = 0; i < N; ++i) {
            auto res = kv.set("key" + std::to_string(i), std::to_string(i));
            if (!res.has_value()) failed = true;
        }
    };
    auto reader = [&](){
        for (int i = 0; i < N; ++i) {
            auto res = kv.get("key" + std::to_string(i));
            if (!res.has_value()) failed = true;
        }
    };
    std::thread t1(writer), t2(reader), t3(writer), t4(reader);
    t1.join(); t2.join(); t3.join(); t4.join();
    EXPECT_FALSE(failed);
    EXPECT_EQ(kv.size(), N);
    for (int i = 0; i < N; ++i) {
        auto res = kv.get("key" + std::to_string(i));
        ASSERT_TRUE(res.has_value());
        ASSERT_TRUE(res.value().has_value());
        EXPECT_EQ(res.value().value(), std::to_string(i));
    }
}

TEST_F(InMemoryKVStoreTest, ThreadSafetyErase) {
    const int N = 100;
    for (int i = 0; i < N; ++i) kv.set("key" + std::to_string(i), "v");
    std::atomic<int> erased{0};
    auto eraser = [&](){
        for (int i = 0; i < N; ++i) {
            auto res = kv.erase("key" + std::to_string(i));
            if (res.has_value() && res.value().has_value()) ++erased;
        }
    };
    std::thread t1(eraser), t2(eraser);
    t1.join(); t2.join();
    EXPECT_EQ(erased, N);
    EXPECT_EQ(kv.size(), 0);
}

TEST_F(InMemoryKVStoreTest, StressTestConcurrentSetGetErase) {
    const int N = 1000;
    std::atomic<bool> failed{false};
    auto worker = [&](int tid){
        for (int i = 0; i < N; ++i) {
            std::string key = "k" + std::to_string(i);
            kv.set(key, std::to_string(i));
            auto res = kv.get(key);
            if (!res.has_value()) failed = true;
            kv.erase(key);
        }
    };
    std::vector<std::thread> threads;
    for (int i = 0; i < 8; ++i) threads.emplace_back(worker, i);
    for (auto& t : threads) t.join();
    EXPECT_FALSE(failed);
    EXPECT_EQ(kv.size(), 0);
}

// Error path tests: Simulate error by using invalid keys/values if possible.
// If Error is only used for future extension, these tests will pass as normal.
// If you add error injection to InMemoryKVStore, add tests here.
