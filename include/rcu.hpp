#ifndef RCU_HPP
#define RCU_HPP

#include "task.hpp"
#include <atomic>

namespace rcu {

void rcu_read_lock();
void rcu_read_unlock();

void rcu_assign_pointer(void*, void*);
task<void> rcu_sync_run();

}  // namespace rcu

#endif  // RCU_HPP