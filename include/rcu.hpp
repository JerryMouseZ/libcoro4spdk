#ifndef RCU_HPP
#define RCU_HPP

#include "task.hpp"
#include <atomic>

namespace pmss {
namespace rcu {

struct rcu_head {
  struct rcu_head* next;
  unsigned long version;
  void (*func)(struct rcu_head* head);
};

struct call_rcu_data {
  rcu_head* head;
  rcu_head* tail;
  unsigned long count;
};

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

task<void> synchronize_rcu();

void rcu_init();

void rcu_offline();

void thread_call_rcu();
}  // namespace rcu
}  // namespace pmss
#endif  // RCU_HPP
