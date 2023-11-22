#include "rcu.hpp"
#include "conditionvariable.hpp"
// #include "spinlock.hpp"
#include "mutex.hpp"

namespace rcu {

std::atomic<int> cnt[2];  // reader count
std::atomic<int> mark(0);

thread_local int write_pointer = 0;
thread_local int read_pointer = 0;

async_simple::coro::Notifier _cv[2];

void rcu_read_lock() {
  read_pointer = mark.load(std::memory_order_acquire);
  if (cnt[read_pointer].fetch_add(1, std::memory_order_acquire) == 0) {
    _cv[read_pointer].reset();
  }
}

void rcu_read_unlock() {
  if (cnt[read_pointer].fetch_sub(1, std::memory_order_acquire) == 1) {
    _cv[read_pointer].notify();
  }
}

void rcu_assign_pointer(void* p, void* v) {
  *(uint64_t*)p = *(uint64_t*)v;
  std::atomic_thread_fence(std::memory_order_release);
  write_pointer = mark.fetch_xor(1, std::memory_order_acquire);
}

task<void> rcu_sync_run() {
  _cv[write_pointer].reset();
  if (cnt[write_pointer].load(std::memory_order_acquire) != 0) {
    co_await _cv[write_pointer].wait();
  }
}

}  // namespace rcu
