#include "rcu.hpp"
#include <algorithm>
#include <atomic>
#include "schedule.hpp"
#include "spdk/env.h"

namespace pmss {
namespace rcu {

std::atomic<unsigned long> versions[256];
std::atomic<unsigned long> sequencer = 0;
const static unsigned long DONE = LONG_LONG_MAX;
thread_local int rcu_count = 1023;

void rcu_init() {
  std::fill(versions, versions + 256, LONG_LONG_MAX);
}

void rcu_read_lock() {
  ++rcu_count;
  if (rcu_count == 1024) [[unlikely]] {
    rcu_count = 0;
    int current_core = spdk_env_get_current_core();
    unsigned long global_version = sequencer.load(std::memory_order_acquire);
    if (global_version ==
        versions[current_core].load(std::memory_order_relaxed))
      return;
    versions[current_core].store(global_version, std::memory_order_release);
    std::atomic_thread_fence(std::memory_order_seq_cst);
  }
}

void rcu_read_unlock() {}

void rcu_offline() {
  int current_core = spdk_env_get_current_core();
  versions[current_core].store(DONE, std::memory_order_release);
  rcu_count = 1023;
}

task<void> synchronize_rcu() {
  unsigned long writer_version =
      sequencer.fetch_add(1, std::memory_order_acquire) + 1;
  int current_core = spdk_env_get_current_core();
  for (int i = 0; i < num_threads; ++i) {
    if (i == current_core)
      continue;
    while (writer_version > versions[i].load(std::memory_order_acquire)) {
      co_await yield();
    }
  }
}

thread_local call_rcu_data rcu_data;
unsigned long rcu_data_enqueue(struct call_rcu_data* data,
                               struct rcu_head* head) {
  if (data->tail == nullptr) {
    data->head = data->tail = head;
  } else {
    data->tail->next = head;
    data->tail = head;
  }
  ++data->count;
  return data->count;
}

// check current stage memory
// if large trigger memory relaim
// else push it to queue or launch a coroutine to free it
// we let it a coroutine since it may free memory immediately when it has used lots of memory
void call_rcu(struct rcu_head* head, void (*func)(struct rcu_head* head)) {
  // assume that the address of the rcu_head is the same as the object
  unsigned long writer_version =
      sequencer.fetch_add(1, std::memory_order_acquire) + 1;
  head->version = writer_version;
  head->func = func;
  unsigned long cnt = rcu_data_enqueue(&rcu_data, head);
  if (cnt >= 1024)
    thread_call_rcu();
}

void _free_rcu(struct rcu_head* head) {
  // assume head is the first member of the structure
  free((void*)head);
}

void free_rcu(struct rcu_head* head) {
  call_rcu(head, _free_rcu);
}

void thread_call_rcu() {
  // free memory call by the thread
  int current_core = spdk_env_get_current_core();
  unsigned long min_version = UINT_MAX;
  for (int i = 0; i < num_threads; ++i) {
    if (i == current_core)
      continue;
    min_version =
        std::min(min_version, versions[i].load(std::memory_order_acquire));
  }

  rcu_head* node = rcu_data.head;
  while (node) {
    rcu_head* head = node;
    node = node->next;
    if (head->version > min_version)
      break;
    head->func(head);
    rcu_data.head = node;
    if (node == nullptr) {
      rcu_data.tail = nullptr;
    }
  }
}

}  // namespace rcu
}  // namespace pmss
