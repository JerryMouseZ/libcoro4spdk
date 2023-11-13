#include "rcu.hpp"
#include "spinlock.hpp"
#include "conditionvariable.hpp"

namespace rcu {

    std::atomic<int> _cnt(0);
    async_simple::coro::SpinLock _spinlock(128);
    async_simple::coro::ConditionVariable<async_simple::coro::SpinLock> _cv;

    void rcu_read_lock() {
        _cnt.fetch_add(1, std::memory_order_acquire);
    }

    void rcu_read_unlock() {
        if (_cnt.fetch_sub(1, std::memory_order_acquire) == 1) {
            _cv.notify();
        }
    }

    task<void> rcu_sync_run() {
        co_await _spinlock.coLock();
        co_await _cv.wait(_spinlock, [&]{ return _cnt.load(std::memory_order_acquire)==0; });
        _spinlock.unlock();
    }

}   // namespace rcu