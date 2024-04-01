#include "spinlock.hpp"
#include "task.hpp"
#include <gtest/gtest.h>
#include "common.hpp"

const int n_reactor = 1;
const int n_round = 100000;

int i = 0;
async_simple::coro::SpinLock spinlock;

task<int> spinlock_test() {
  for (int j = 0; j < n_round; j++) {
    co_await spinlock.coLock();
    i++;
    spinlock.unlock();
  }
  co_return 0;
}

TEST(spinlock, simple_lock) {
  pmss::init_service(n_reactor, json_file, bdev_dev);
  for (int i = 0; i < n_reactor; i++) {
    pmss::add_task(spinlock_test());
  }
  pmss::run();
  EXPECT_TRUE(i == n_reactor * n_round);
  pmss::deinit_service();
}
