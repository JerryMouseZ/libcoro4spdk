#ifndef RCU_HASH_TABLE_HPP
#define RCU_HASH_TABLE_HPP

#include "../cds/rculist.h"
#include "rcu.hpp"
#include "mutex.hpp"
#include "task.hpp"
#include <vector>
#include <functional>
#include <atomic>
#include <stdexcept>
#include <optional>
#include <cstddef>
#include <utility>

template <typename K, typename V>
struct RcuHashTableNode {
    K key;
    V value;
    ListNode list_node; // Intrusive list node
    pmss::rcu::rcu_head rcu_head_node; // For deferred destruction of the node itself

    RcuHashTableNode(K k, V v) : key(std::move(k)), value(std::move(v)) {}

    static void deferred_free_node(pmss::rcu::rcu_head* head) {
        RcuHashTableNode<K,V>* node_ptr = reinterpret_cast<RcuHashTableNode<K,V>*>(
            reinterpret_cast<char*>(head) - offsetof(RcuHashTableNode<K,V>, rcu_head_node)
        );
        delete node_ptr;
    }
};

template <typename K, typename V, typename Hash = std::hash<K>, typename KeyEqual = std::equal_to<K>>
class RcuHashTable {
public:
    using Node = RcuHashTableNode<K, V>;
    static constexpr double MAX_LOAD_FACTOR = 0.75;

private:
    struct TableInternal {
        std::vector<ListNode> buckets;
        size_t capacity;
        std::atomic<size_t> item_count; // item_count for this specific table instance
        pmss::rcu::rcu_head rcu_head_table; // For RCU freeing of this TableInternal structure

        TableInternal(size_t cap) : capacity(cap), item_count(0) {
            buckets.resize(capacity);
            for (size_t i = 0; i < capacity; ++i) {
                buckets[i].next = &buckets[i];
                buckets[i].prev = &buckets[i];
            }
        }

        // Callback for pmss::rcu::call_rcu to free the TableInternal structure
        static void deferred_free_table(pmss::rcu::rcu_head* head) {
            TableInternal* table_internal_ptr = reinterpret_cast<TableInternal*>(
                reinterpret_cast<char*>(head) - offsetof(TableInternal, rcu_head_table)
            );
            // This only deletes the TableInternal structure (e.g., the buckets vector).
            // Nodes that were part of this table must have been moved to a new table
            // or individually scheduled for freeing by other operations (like remove or clear).
            delete table_internal_ptr;
        }
    };

    std::atomic<TableInternal*> table_ptr;
    Hash hash_fn;
    KeyEqual key_eq_fn;
    async_simple::coro::Mutex write_mutex;

    // Private resize helper, assumes write_mutex is held by caller.
    async_simple::Task<void> resize_under_lock_unsafe(size_t new_capacity);

public:
    RcuHashTable() : table_ptr(nullptr) {}

    ~RcuHashTable() {
        // Manual destroy() is expected.
        // If table_ptr is not null here, and destroy() hasn't been called,
        // the TableInternal and any nodes would be leaked.
    }

    void init(size_t initial_capacity = 16) {
        if (initial_capacity == 0) throw std::invalid_argument("Initial capacity must be > 0.");
        if (table_ptr.load(std::memory_order_relaxed) != nullptr) throw std::runtime_error("Already initialized.");
        TableInternal* new_table = new TableInternal(initial_capacity);
        table_ptr.store(new_table, std::memory_order_release);
    }

    async_simple::Task<void> destroy() {
        auto lock = co_await write_mutex.coScopedLock();
        TableInternal* current_table_to_free = table_ptr.load(std::memory_order_acquire);
        table_ptr.store(nullptr, std::memory_order_release);
        lock.unlock();

        if (current_table_to_free) {
            // Note: This destroy does not iterate and free individual nodes.
            // It assumes a clear() method would handle that, or nodes are removed manually.
            // If TableInternal is freed via call_rcu, deferred_free_table is called.
            // If deleted directly after synchronize_rcu, its destructor runs.
            // For consistency with resize, let's use call_rcu for TableInternal if possible.
            // However, the original plan was direct delete after synchronize_rcu for destroy.
            // Let's stick to that for destroy() for now to match earlier steps,
            // but acknowledge that resize uses call_rcu for old tables.
            co_await pmss::rcu::synchronize_rcu();
            delete current_table_to_free;
        }
        co_return;
    }

    std::optional<V> lookup(const K& key) const {
        pmss::rcu::rcu_read_lock();
        TableInternal* current_table = table_ptr.load(std::memory_order_acquire);
        if (!current_table || current_table->capacity == 0) {
            pmss::rcu::rcu_read_unlock();
            return std::nullopt;
        }
        size_t bucket_idx = hash_fn(key) % current_table->capacity;
        const ListNode* bucket_head = &current_table->buckets[bucket_idx];
        ListNode* head_next_ptr = const_cast<ListNode*>(bucket_head->next);
        const ListNode* pos = pmss::rcu::rcu_dereference(head_next_ptr);
        while (pos != bucket_head) {
            const Node* node = reinterpret_cast<const Node*>(
                reinterpret_cast<const char*>(pos) - offsetof(Node, list_node));
            if (key_eq_fn(node->key, key)) {
                V value_copy = node->value;
                pmss::rcu::rcu_read_unlock();
                return value_copy;
            }
            ListNode* current_pos_next_ptr = const_cast<ListNode*>(pos->next);
            pos = pmss::rcu::rcu_dereference(current_pos_next_ptr);
        }
        pmss::rcu::rcu_read_unlock();
        return std::nullopt;
    }

    async_simple::Task<bool> insert(const K& key, const V& value) {
        auto lock = co_await write_mutex.coScopedLock();
        TableInternal* table_at_start_of_insert = table_ptr.load(std::memory_order_acquire);

        if (!table_at_start_of_insert) {
            throw std::runtime_error("Table not initialized. Call init() before insert.");
        }

        if (table_at_start_of_insert->item_count.load(std::memory_order_relaxed) >=
            static_cast<size_t>(static_cast<double>(table_at_start_of_insert->capacity) * MAX_LOAD_FACTOR) &&
            table_at_start_of_insert->capacity > 0) {
            size_t new_capacity = table_at_start_of_insert->capacity * 2;
            // Ensure new_capacity is at least a minimal value if current capacity is very small (e.g. 1)
            if (new_capacity <= table_at_start_of_insert->capacity) new_capacity = table_at_start_of_insert->capacity + 1;
            if (new_capacity == 0) new_capacity = 16; // Should be caught by capacity > 0 or previous line

            co_await resize_under_lock_unsafe(new_capacity);
        }

        TableInternal* current_table = table_ptr.load(std::memory_order_acquire); // Reload after potential resize
        if (!current_table) {
            throw std::runtime_error("Table is null after potential resize operation.");
        }

        size_t bucket_idx = hash_fn(key) % current_table->capacity;
        ListNode* bucket_head_ptr = &current_table->buckets[bucket_idx];

        ListNode* pos_check = bucket_head_ptr->next;
        while (pos_check != bucket_head_ptr) {
            Node* node_check = reinterpret_cast<Node*>(
                reinterpret_cast<char*>(pos_check) - offsetof(Node, list_node));
            if (key_eq_fn(node_check->key, key)) {
                co_return false;
            }
            pos_check = pos_check->next;
        }

        Node* new_node = new Node(key, value);
        cds_list_add_rcu(&new_node->list_node, bucket_head_ptr);
        current_table->item_count.fetch_add(1, std::memory_order_relaxed);

        co_return true;
    }

    async_simple::Task<bool> remove(const K& key) {
        auto lock = co_await write_mutex.coScopedLock();
        TableInternal* current_table = table_ptr.load(std::memory_order_acquire);

        if (!current_table || current_table->capacity == 0) {
            co_return false;
        }

        size_t bucket_idx = hash_fn(key) % current_table->capacity;
        ListNode* bucket_head_ptr = &current_table->buckets[bucket_idx];

        ListNode* current_list_iter = bucket_head_ptr->next;
        while (current_list_iter != bucket_head_ptr) {
            Node* node_to_check = reinterpret_cast<Node*>(
                reinterpret_cast<char*>(current_list_iter) - offsetof(Node, list_node)
            );
            if (key_eq_fn(node_to_check->key, key)) {
                cds_list_del_rcu(&node_to_check->list_node);
                current_table->item_count.fetch_sub(1, std::memory_order_relaxed);
                pmss::rcu::call_rcu(&node_to_check->rcu_head_node, Node::deferred_free_node);
                co_return true;
            }
            current_list_iter = current_list_iter->next;
        }
        co_return false;
    }

    async_simple::Task<void> resize_under_lock_unsafe(size_t new_capacity) {
        // Precondition: write_mutex is held by the caller.
        if (new_capacity == 0) {
            throw std::invalid_argument("New capacity cannot be zero in resize_under_lock_unsafe.");
        }

        TableInternal* old_table_internal = table_ptr.load(std::memory_order_relaxed);

        if (!old_table_internal) {
            // This should not happen if this method is called correctly (e.g., from insert on an initialized table).
            throw std::runtime_error("Cannot resize a null table. Ensure init() was called.");
        }

        // Avoid resizing if capacity is unchanged or if new capacity is too small (e.g. less than current item count)
        // This second check helps prevent shrinking below current data size, though load factor should guide growth.
        size_t current_item_count = old_table_internal->item_count.load(std::memory_order_relaxed);
        if (new_capacity == old_table_internal->capacity || new_capacity < current_item_count) {
            // If new_capacity < current_item_count, it might indicate a logic error or a need for a shrink strategy.
            // For now, just returning avoids making things worse. A proper shrink might be different.
            co_return;
        }

        TableInternal* new_table_internal = new TableInternal(new_capacity);
        // item_count for new_table_internal is already 0 by its constructor. It will be incremented as nodes are added.

        // Rehash all nodes from old_table_internal to new_table_internal
        for (size_t i = 0; i < old_table_internal->capacity; ++i) {
            ListNode* old_bucket_head = &old_table_internal->buckets[i];
            ListNode* current_node_ptr_in_old_bucket = old_bucket_head->next;
            while (current_node_ptr_in_old_bucket != old_bucket_head) {
                Node* node_to_move = reinterpret_cast<Node*>(
                    reinterpret_cast<char*>(current_node_ptr_in_old_bucket) - offsetof(Node, list_node)
                );
                // Save next pointer before modifying list_node for re-adding
                ListNode* next_node_in_old_bucket = current_node_ptr_in_old_bucket->next;

                // Calculate new bucket index in the new table
                size_t new_bucket_idx = hash_fn(node_to_move->key) % new_table_internal->capacity;
                ListNode* new_bucket_head = &new_table_internal->buckets[new_bucket_idx];

                // Add node to the new table's bucket.
                // Since new_table_internal is not yet published, a direct list manipulation would be safe.
                // Using cds_list_add_rcu ensures list_node is RCU-ready for when new_table_internal becomes live.
                // Note: cds_list_add_rcu adds to the head of the list.
                cds_list_add_rcu(&node_to_move->list_node, new_bucket_head);
                new_table_internal->item_count.fetch_add(1, std::memory_order_relaxed); // Increment count in new table

                current_node_ptr_in_old_bucket = next_node_in_old_bucket;
            }
        }

        // Ensure old table's item count is consistent with what was moved, for clarity, though it's being discarded.
        // old_table_internal->item_count.store(0, std::memory_order_relaxed);


        // Publish the new table. This makes it visible to all subsequent operations (readers and writers).
        // This is the RCU equivalent of an atomic pointer assignment.
        table_ptr.store(new_table_internal, std::memory_order_release); // memory_order_release ensures prior writes are visible

        // Schedule the old TableInternal structure (not the nodes, they were moved) for deferred deletion.
        // This uses the rcu_head_table member of the old TableInternal.
        pmss::rcu::call_rcu(&old_table_internal->rcu_head_table, TableInternal::deferred_free_table);

        co_return;
    }
};

#endif // RCU_HASH_TABLE_HPP
