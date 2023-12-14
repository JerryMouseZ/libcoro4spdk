#include "rcu.hpp"
#include <algorithm>
#include <atomic>
#include <climits>
#include "schedule.hpp"
#include "conditionvariable.hpp"
#include "mutex.hpp"

namespace pmss {
namespace rcu {

std::atomic<unsigned long> versions[256];
std::atomic<unsigned long> sequencer = 0;
const static unsigned long DONE = LONG_LONG_MAX;
unsigned long writer_version;
unsigned long oldest_version = 0;
thread_local int first_rcu = 0;
thread_local int rcu_count = 0;

void rcu_init() {
  std::fill(versions, versions + 256, LONG_LONG_MAX);
}

// does it need memory barrier for reader?
// barrier is faster: maybe
void rcu_read_lock() {
  if (first_rcu == 0) [[unlikely]] {
    int current_core = spdk_env_get_current_core();
    first_rcu++;
    versions[current_core].store(sequencer.load(std::memory_order_acquire),
                                 std::memory_order_release);
    std::atomic_thread_fence(std::memory_order_seq_cst);
    return;
  }
  rcu_count++;
  if (rcu_count == 1023) {
    int current_core = spdk_env_get_current_core();
    versions[current_core].store(sequencer.load(std::memory_order_acquire),
                                 std::memory_order_release);
    std::atomic_thread_fence(std::memory_order_seq_cst);
    rcu_count = 0;
  }
  /* if (first_rcu == 1) [[unlikely]] {} */
}

void rcu_read_unlock() {
  /* int current_core = spdk_env_get_current_core(); */
  /* versions[current_core].store(DONE, std::memory_order_release); */
}

void rcu_offline() {
  int current_core = spdk_env_get_current_core();
  versions[current_core].store(DONE, std::memory_order_release);
}

void update_oldest() {
  oldest_version = DONE;
  for (int i = 0; i < num_threads; ++i) {
    oldest_version =
        std::min(versions[i].load(std::memory_order_acquire), oldest_version);
  }
}

task<void> rcu_sync_run() {
  writer_version = sequencer.fetch_add(1, std::memory_order_acq_rel) + 1;
  update_oldest();
  while (writer_version > oldest_version) {
    co_await yield();
    update_oldest();
  }
}

}  // namespace rcu
}  // namespace pmss
