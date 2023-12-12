#include "rcu.hpp"
#include <algorithm>
#include <atomic>
#include <climits>
#include "schedule.hpp"
#include "mutex.hpp"

namespace pmss {
namespace rcu {

std::atomic<unsigned long> versions[256];
std::atomic<unsigned long> sequencer = 0;
const static unsigned long DONE = LONG_LONG_MAX;
unsigned long writer_version;
unsigned long oldest_version = 0;

void rcu_init() {
  std::fill(versions, versions + 256, LONG_LONG_MAX);
}

// does it need memory barrier for reader?
// barrier is faster: maybe
void rcu_read_lock() {
  int current_core = spdk_env_get_current_core();
  versions[current_core].store(sequencer.load(std::memory_order_acquire),
                               std::memory_order_release);
}

void rcu_read_unlock() {
  int current_core = spdk_env_get_current_core();
  versions[current_core].store(DONE, std::memory_order_release);
}

task<void> synchronize_rcu() {
  for (int i = 0; i < num_threads; ++i) {
    while (writer_version > versions[i].load(std::memory_order_acquire))
      co_await yield();
  }
}

}  // namespace rcu
}  // namespace pmss
