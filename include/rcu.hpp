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
  *((volatile T**)&p) = v;
}

template <typename T>
static inline T* rcu_dereference(T*& p) {
  return (T*)*((volatile T**)&p);
}

task<void> rcu_sync_run();

void rcu_init();

void rcu_offline();

}  // namespace rcu
}  // namespace pmss
#endif  // RCU_HPP
