#ifndef RCU_HPP
#define RCU_HPP

#include "task.hpp"
#include <atomic>

namespace pmss {
namespace rcu {

void rcu_read_lock();
void rcu_read_unlock();

void rcu_assign_pointer(void*& p, void* v);

task<void> rcu_sync_run();

void rcu_init();

}  // namespace rcu
}  // namespace pmss
#endif  // RCU_HPP
