#include "rcu.hpp"
#include <algorithm>
#include <atomic>
#include <climits>
#include "schedule.hpp"
#include "conditionvariable.hpp"
// #include "spinlock.hpp"
#include "mutex.hpp"

namespace pmss {
namespace rcu {

std::atomic<unsigned long> versions[256];
std::atomic<unsigned long> sequencer = 0;
const static unsigned long DONE = LONG_MAX;
std::atomic<unsigned long long> writer_version;
unsigned long oldest_version = 0;

void rcu_init() {
  std::fill(versions, versions + 256, LONG_LONG_MAX);
}

// does it need memory barrier for reader?
void rcu_read_lock() {
  int current_core = spdk_env_get_current_core();
  versions[current_core].store(
      sequencer.fetch_add(1, std::memory_order_acquire),
      std::memory_order_release);
}

void rcu_read_unlock() {
  int current_core = spdk_env_get_current_core();
  versions[current_core].store(DONE, std::memory_order_release);
}

void update_oldest() {
  for (int i = 0; i < num_threads; ++i) {
    // 顺序不重要，大不了多yield一次
    oldest_version =
        std::min(versions[i].load(std::memory_order_relaxed), oldest_version);
  }
}

task<void> rcu_sync_run() {
  while (writer_version >= oldest_version) {
    co_await yield();
    update_oldest();
  }
}

}  // namespace rcu
}  // namespace pmss
