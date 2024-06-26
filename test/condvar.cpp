#include "conditionvariable.hpp"
#include "spinlock.hpp"
#include "task.hpp"
#include <gtest/gtest.h>
#include "common.hpp"

async_simple::coro::SpinLock spinlock;
async_simple::coro::ConditionVariable<async_simple::coro::SpinLock> cv;
int value = 0;

task<int> producer_1() {
  co_await spinlock.coLock();
  value++;
  cv.notify();
  spinlock.unlock();
  co_return 0;
}

task<int> consumer_1() {
  co_await spinlock.coLock();
  co_await cv.wait(spinlock, [&] { return value > 0; });
  spinlock.unlock();
  assert(value > 0);
  co_return 0;
}

TEST(condvar_test_1, subtest_1) {
  pmss::init_service(2, json_file, bdev_dev);
  pmss::add_task(producer_1());
  pmss::add_task(consumer_1());
  pmss::run();
  pmss::deinit_service();
}

async_simple::coro::Notifier notifier;  // Notifier = ConditionVariable<void>
bool ready = false;

task<int> producer_2() {
  ready = true;
  notifier.notify();
  co_return 0;
}

task<int> consumer_2() {
  co_await notifier.wait();
  assert(ready);
  co_return 0;
}

TEST(condvar_test_2, subtest_2) {
  pmss::init_service(2, json_file, bdev_dev);
  pmss::add_task(producer_2());
  pmss::add_task(consumer_2());
  pmss::run();
  pmss::deinit_service();
}
