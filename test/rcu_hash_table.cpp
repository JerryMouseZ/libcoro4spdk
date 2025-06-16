#include "gtest/gtest.h"
#include "../include/rcu_hash_table.hpp"
#include "../include/task.hpp"
#include "../include/schedule.hpp"
#include "../include/rcu.hpp"
#include <string>
#include <vector>
#include <atomic>
#include <iostream> // For debug printing if needed
#include <optional> // For std::optional in tests

// Basic Test Fixture for RcuHashTable
class RcuHashTableTest : public ::testing::Test {
protected:
    RcuHashTable<int, std::string>* table_ptr;

    void SetUp() override {
        // These might need to be valid paths or SPDK device names in a real test environment.
        // Using placeholder names as per the prompt.
        pmss::init_service(3, "test_bdev.json", "rcu_hash_table_test_dev");
        table_ptr = new RcuHashTable<int, std::string>();
        table_ptr->init(4); // Initial capacity of 4 for easier resize trigger
    }

    void TearDown() override {
        if (table_ptr) {
            auto destroy_task = table_ptr->destroy();
            // Create a new task to await the destruction, then run the scheduler.
            pmss::add_task([&]() -> async_simple::Task<void> {
                co_await std::move(destroy_task);
            }());
            pmss::run(); // Process the destroy task and any RCU callbacks
            delete table_ptr;
            table_ptr = nullptr;
        }
        pmss::deinit_service();
    }
};

TEST_F(RcuHashTableTest, InitAndDestroy) {
    // SetUp and TearDown handle this. If they pass, this test passes.
    SUCCEED();
}

TEST_F(RcuHashTableTest, SimpleInsertAndLookup) {
    bool inserted = false;
    pmss::add_task([&]() -> async_simple::Task<void> {
        inserted = co_await table_ptr->insert(1, "one");
    }());
    pmss::run();
    ASSERT_TRUE(inserted);

    auto result = table_ptr->lookup(1);
    ASSERT_TRUE(result.has_value());
    ASSERT_EQ(result.value(), "one");

    auto missing_result = table_ptr->lookup(2);
    ASSERT_FALSE(missing_result.has_value());
}

TEST_F(RcuHashTableTest, InsertExistingKey) {
    bool inserted1 = false, inserted2 = false;
    pmss::add_task([&]() -> async_simple::Task<void> {
        inserted1 = co_await table_ptr->insert(10, "ten");
        inserted2 = co_await table_ptr->insert(10, "another ten"); // Should fail
    }());
    pmss::run();
    ASSERT_TRUE(inserted1);
    ASSERT_FALSE(inserted2); // Expecting false as key 10 already exists

    auto result = table_ptr->lookup(10);
    ASSERT_TRUE(result.has_value());
    ASSERT_EQ(result.value(), "ten"); // Value should be the original "ten"
}

TEST_F(RcuHashTableTest, RemoveExistingKey) {
    bool inserted = false, removed = false, removed_again = false;
    std::optional<std::string> result_before_remove, result_after_remove;

    pmss::add_task([&]() -> async_simple::Task<void> {
        inserted = co_await table_ptr->insert(20, "twenty");
        result_before_remove = table_ptr->lookup(20); // Lookup before remove
        removed = co_await table_ptr->remove(20);

        // After remove, the node is unlinked and scheduled for RCU deletion.
        // A synchronize_rcu() call helps ensure the grace period for deletion callback execution
        // before we try to lookup again. This makes the test more deterministic for lookup after remove.
        co_await pmss::rcu::synchronize_rcu();

        result_after_remove = table_ptr->lookup(20); // Lookup after remove and grace period
        removed_again = co_await table_ptr->remove(20); // Try removing again, should fail
    }());
    pmss::run();

    ASSERT_TRUE(inserted);
    ASSERT_TRUE(result_before_remove.has_value());
    ASSERT_EQ(result_before_remove.value(), "twenty");
    ASSERT_TRUE(removed);
    ASSERT_FALSE(result_after_remove.has_value()); // Expect not found after remove and grace period
    ASSERT_FALSE(removed_again); // Expect false as key 20 is already removed
}

TEST_F(RcuHashTableTest, RemoveNonExistingKey) {
    bool removed = true; // Initialize to true to ensure it's changed by the async task
    pmss::add_task([&]() -> async_simple::Task<void> {
        removed = co_await table_ptr->remove(99); // Key 99 does not exist
    }());
    pmss::run();
    ASSERT_FALSE(removed);
}

TEST_F(RcuHashTableTest, ResizeOnInsert) {
    bool ins[5] = {false}; // To store insertion results
    pmss::add_task([&]() -> async_simple::Task<void> {
        // Initial capacity is 4. MAX_LOAD_FACTOR = 0.75.
        // Resize should trigger when item_count >= 4 * 0.75 = 3.
        // 1. Insert (1, "item1"): count becomes 1. (1 < 3) No resize.
        // 2. Insert (2, "item2"): count becomes 2. (2 < 3) No resize.
        // 3. Insert (3, "item3"): count becomes 3. (3 >= 3) Resize should be triggered by this insertion.
        //    The check is `item_count >= capacity * load_factor`.
        //    Before inserting key 3, item_count is 2. 2 < 3. No resize. Insert key 3. item_count becomes 3.
        //    Next insert, key 4: item_count is 3. 3 >= 3. Resize to 8. Then insert key 4.
        ins[0] = co_await table_ptr->insert(1, "item1");
        ins[1] = co_await table_ptr->insert(2, "item2");
        ins[2] = co_await table_ptr->insert(3, "item3");
        // At this point, item_count = 3. Capacity = 4. 3 >= (4*0.75=3).
        // So, the *next* insert should trigger resize.
        ins[3] = co_await table_ptr->insert(4, "item4"); // This insert should trigger resize to capacity 8.
        ins[4] = co_await table_ptr->insert(5, "item5"); // Inserted into resized table.
    }());
    pmss::run();

    for(int i=0; i<5; ++i) ASSERT_TRUE(ins[i]) << "Insertion failed for item " << (i+1);

    // Verify all items are present
    ASSERT_EQ(table_ptr->lookup(1).value_or(""), "item1");
    ASSERT_EQ(table_ptr->lookup(2).value_or(""), "item2");
    ASSERT_EQ(table_ptr->lookup(3).value_or(""), "item3");
    ASSERT_EQ(table_ptr->lookup(4).value_or(""), "item4");
    ASSERT_EQ(table_ptr->lookup(5).value_or(""), "item5");
}

TEST_F(RcuHashTableTest, ConcurrentReadWrite) {
    const int num_readers = 3;
    const int items_to_insert = 10;
    std::atomic<int> writer_inserts(0);
    std::atomic<bool> writer_done(false);
    std::vector<std::function<async_simple::Task<void>()>> task_gens; // Use std::function to store task generators

    // Writer task generator
    task_gens.push_back([&]() -> async_simple::Task<void> {
        for (int i = 0; i < items_to_insert; ++i) {
            if (co_await table_ptr->insert(i, "value_" + std::to_string(i))) {
                writer_inserts++;
            }
            if (i % 2 == 0) co_await async_simple::coro::Yield{};
        }
        writer_done.store(true, std::memory_order_release);
    });

    // Reader task generators
    for (int r = 0; r < num_readers; ++r) {
        task_gens.push_back([&]() -> async_simple::Task<void> {
            // Keep reading until writer is done and we've tried to read recently inserted items.
            // This is a best-effort to observe concurrent state.
            int key_to_check = 0;
            while (!writer_done.load(std::memory_order_acquire) || key_to_check < writer_inserts.load(std::memory_order_acquire)) {
                 // Check a cycling key or a random key up to what writer might have inserted
                int current_max_key = writer_inserts.load(std::memory_order_relaxed);
                if (current_max_key > 0) {
                    key_to_check = (key_to_check + 1) % current_max_key;
                     auto res = table_ptr->lookup(key_to_check);
                    if (res.has_value()) {
                        // In a concurrent setting, it's hard to assert exact values if they can be updated.
                        // Here, values are stable after insert.
                        EXPECT_EQ(res.value(), "value_" + std::to_string(key_to_check));
                    }
                }
                co_await async_simple::coro::Yield{};
            }
        });
    }

    for(auto& task_gen_fn : task_gens) {
        pmss::add_task(task_gen_fn()); // Execute the generator to get the task
    }
    pmss::run(); // Run all added tasks

    ASSERT_TRUE(writer_done.load());
    ASSERT_EQ(writer_inserts.load(), items_to_insert);

    // Final verification: all items inserted by writer must be present
    for (int i = 0; i < items_to_insert; ++i) {
        auto res = table_ptr->lookup(i);
        ASSERT_TRUE(res.has_value()) << "Item " << i << " not found after concurrent test.";
        if(res.has_value()) { // Check to prevent dereferencing empty optional
            ASSERT_EQ(res.value(), "value_" + std::to_string(i));
        }
    }
}

// It might be good to add a main function if this file is compiled standalone.
// However, for GTest, it's usually linked with a main provided by GTest itself.
// int main(int argc, char **argv) {
//     ::testing::InitGoogleTest(&argc, argv);
//     return RUN_ALL_TESTS();
// }
