#ifndef RCU_HPP
#define RCU_HPP

#include "task.hpp"
#include <atomic>

namespace pmss {
namespace rcu {

extern std::atomic<unsigned long> sequencer;
extern unsigned long writer_version;
void rcu_read_lock();
void rcu_read_unlock();

template <typename T>
static inline void rcu_assign_pointer(T*& p, T* v) {
  p = v;
  writer_version = sequencer.load(std::memory_order_acquire);
}

task<void> rcu_sync_run();

void rcu_init();

}  // namespace rcu
}  // namespace pmss
#endif  // RCU_HPP
