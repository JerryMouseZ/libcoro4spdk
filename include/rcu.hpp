#ifndef RCU_HPP
#define RCU_HPP

#include "task.hpp"
#include <atomic>

namespace pmss {
namespace rcu {

void rcu_read_lock();
void rcu_read_unlock();
extern std::atomic<unsigned long> sequencer;
extern unsigned long writer_version;

template <typename T>
static inline void rcu_assign_pointer(T*& p, T* v) {
  std::atomic_thread_fence(std::memory_order_seq_cst);
  p = v;
  writer_version = sequencer.load(std::memory_order_relaxed) + 1;
  sequencer.store(writer_version, std::memory_order_release);
}

task<void> synchronize_rcu();

void rcu_init();

template <typename T>
static inline T* rcu_dereference(T*& gp) {
  std::atomic_thread_fence(std::memory_order_seq_cst);
  return gp;
}

}  // namespace rcu
}  // namespace pmss
#endif  // RCU_HPP
