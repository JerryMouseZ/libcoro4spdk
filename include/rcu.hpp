#ifndef RCU_HPP
#define RCU_HPP

#include "task.hpp"
#include <atomic>

namespace rcu {

void rcu_read_lock();
void rcu_read_unlock();

task<void> rcu_sync_run();

#define rcu_assign_pointer(p, v)                         \
  do {                                                   \
    p = v;                                               \
    std::atomic_thread_fence(std::memory_order_release); \
  } while (0)

// #define rcu_assign_pointer(p, v) \
//     do { \
//         std::atomic_thread_fence(std::memory_order_release); \
//         p = v; \
//     } while(0)

// #define rcu_assign_pointer(p, v) \
//     do { \
//         __asm__ __volatile__ ("" : : : "memory");   \
//         p = v; \
//     } while(0)

// #define rcu_assign_pointer(p, v)    \
//     do {    \
//         p = v;  \
//     } while(0)

// #define rcu_assign_pointer(p, v)    \
//     do {    \
//         p = v;  \
//         __asm__ __volatile__ ("" : : : "memory");   \
//     } while(0)

// #define rcu_assign_pointer(p, v)    \
//     do {    \
//         __asm__ __volatile__ ("" : : : "memory");   \
//         p = v;  \
//         __asm__ __volatile__ ("" : : : "memory");   \
//     } while(0)

// #define rcu_assign_pointer(p, v)    \
//     do {    \
//         std::atomic_thread_fence(std::memory_order_acquire);    \
//         p = v;  \
//         std::atomic_thread_fence(std::memory_order_release); \
//     } while(0)

}  // namespace rcu

#endif  // RCU_HPP