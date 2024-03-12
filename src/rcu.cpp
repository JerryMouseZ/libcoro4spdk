#include "rcu.hpp"
#include <algorithm>
#include <atomic>
#include <climits>
#include "schedule.hpp"
#include "conditionvariable.hpp"
#include "mutex.hpp"
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
  if (rcu_count == 1024) {
    int current_core = spdk_env_get_current_core();
    versions[current_core].store(sequencer.load(std::memory_order_acquire),
                                 std::memory_order_release);
    std::atomic_thread_fence(std::memory_order_seq_cst);
    rcu_count = 0;
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

}  // namespace rcu
}  // namespace pmss
