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
  /* std::atomic_thread_fence(std::memory_order_seq_cst); */
  p = v;
}

task<void> synchronize_rcu();

void rcu_init();

void rcu_deinit(int threadi);

template <typename T>
static inline T* rcu_dereference(T*& gp) {
  std::atomic_thread_fence(std::memory_order_seq_cst);
  std::atomic<T*> loader(gp);
  return loader.load(std::memory_order_consume);
}

}  // namespace rcu
}  // namespace pmss
#endif  // RCU_HPP
