#include "concurrent_hash_map.hpp"

#include <cassert>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

void test_basic_operations() {
    ConcurrentHashMap<int, std::string> map(4);

    assert(map.insert(1, "one"));
    assert(!map.insert(1, "duplicate"));

    std::string value;
    assert(map.find(1, value));
    assert(value == "one");

    assert(!map.find(2, value));
    assert(!map.update(2, "two"));

    assert(map.update(1, "updated"));
    assert(map.find(1, value));
    assert(value == "updated");

    assert(map.erase(1));
    assert(!map.erase(1));
    assert(!map.find(1, value));
    assert(map.size() == 0);
}

void test_concurrent_inserts() {
    ConcurrentHashMap<int, int> map(8);

    constexpr int thread_count = 8;
    constexpr int values_per_thread = 1000;

    std::vector<std::thread> threads;

    for (int thread_id = 0; thread_id < thread_count; ++thread_id) {
        threads.emplace_back([&map, thread_id]() {
            int start = thread_id * values_per_thread;

            for (int i = 0; i < values_per_thread; ++i) {
                assert(map.insert(start + i, start + i));
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    assert(map.size() == thread_count * values_per_thread);

    for (int i = 0; i < thread_count * values_per_thread; ++i) {
        int value = -1;
        assert(map.find(i, value));
        assert(value == i);
    }
}

void test_same_keys() {
    ConcurrentHashMap<int, int> map;

    for (int i = 0; i < 100; ++i) {
        assert(map.insert(i, 0));
    }

    constexpr int thread_count = 8;
    std::vector<std::thread> threads;

    for (int thread_id = 0; thread_id < thread_count; ++thread_id) {
        threads.emplace_back([&map, thread_id]() {
            for (int i = 0; i < 100; ++i) {
                map.update(i, thread_id);
            }

            for (int i = 0; i < 100; ++i) {
                int value = -1;
                assert(map.find(i, value));
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    assert(map.size() == 100);
}

void test_resize_during_operations() {
    ConcurrentHashMap<int, int> map(2);

    constexpr int thread_count = 8;
    constexpr int values_per_thread = 500;

    std::vector<std::thread> threads;

    for (int thread_id = 0; thread_id < thread_count; ++thread_id) {
        threads.emplace_back([&map, thread_id]() {
            int start = thread_id * values_per_thread;

            for (int i = 0; i < values_per_thread; ++i) {
                int key = start + i;
                map.insert(key, key);

                int value = -1;
                map.find(key, value);
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    assert(map.size() == thread_count * values_per_thread);

    for (int i = 0; i < thread_count * values_per_thread; ++i) {
        int value = -1;
        assert(map.find(i, value));
        assert(value == i);
    }

    map.rehash(4096);

    for (int i = 0; i < thread_count * values_per_thread; ++i) {
        int value = -1;
        assert(map.find(i, value));
        assert(value == i);
    }
}

void run_test(const char* name, void (*test)()) {
    test();
    std::cout << "[PASS] " << name << '\n';
}

int main() {
    run_test("basic operations", test_basic_operations);
    run_test("concurrent inserts", test_concurrent_inserts);
    run_test("same-key access", test_same_keys);
    run_test("resize during concurrent operations",
             test_resize_during_operations);

    std::cout << "All 4 tests passed.\n";
    return 0;
}