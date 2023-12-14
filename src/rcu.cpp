#include "rcu.hpp"
#include <algorithm>
#include <atomic>
#include <climits>
#include "schedule.hpp"
#include "mutex.hpp"
#include "spdk/env.h"

namespace pmss {
namespace rcu {

std::atomic<unsigned long> versions[256];
std::atomic<int> qsbr_iters[256];
std::atomic<unsigned long> sequencer = 1;
/* const static unsigned long DONE = LONG_LONG_MAX; */

void rcu_init() {
  // let all the threads online
  std::fill(versions, versions + 256, 1);
}

void rcu_deinit(int threadi) {
  versions[threadi].store(0, std::memory_order_release);
}

// does it need memory barrier for reader?
// barrier is faster: maybe
void rcu_read_lock() {
  /* std::atomic_thread_fence(std::memory_order_seq_cst); */
}

void qsbr() {
  int current_core = spdk_env_get_current_core();
}

void rcu_read_unlock() {
  /* qsbr(); */
  /* return; */
  int current_core = spdk_env_get_current_core();
  qsbr_iters[current_core]++;
  if (qsbr_iters[current_core] == 128) [[unlikely]] {
    qsbr_iters[current_core] = 0;
    std::atomic_thread_fence(std::memory_order_seq_cst);
    versions[current_core].store(sequencer.load(std::memory_order_acquire),
                                 std::memory_order_release);
    std::atomic_thread_fence(std::memory_order_seq_cst);
  }
}

task<void> synchronize_rcu() {
  unsigned long writer_version =
      sequencer.fetch_add(2, std::memory_order_release) + 2;
  int core = spdk_env_get_current_core();
  for (int i = 0; i < num_threads; ++i) {
    if (i == core)
      continue;
    do {
      unsigned long version = versions[i].load(std::memory_order_acquire);
      if (version == 0 || writer_version == version)
        break;
      co_await yield();
    } while (1);
  }
}

}  // namespace rcu
}  // namespace pmss
