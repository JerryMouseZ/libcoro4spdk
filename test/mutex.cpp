#include "mutex.hpp"
#include "schedule.hpp"
#include "task.hpp"
#include <gtest/gtest.h>
#include "common.hpp"
const int n_reactor = 8;
const int n_round = 100000;

int i = 0;
async_simple::coro::Mutex mutex;

task<int> lock_test() {
  for (int j = 0; j < n_round; ++j) {
    co_await mutex.coLock();
    i++;
    mutex.unlock();
  }
  co_return 0;
}

TEST(mutex_lock, simple_lock) {
  pmss::init_service(n_reactor, json_file, bdev_dev);
  for (int i = 0; i < n_reactor; ++i) {
    pmss::add_task(lock_test());
  }
  pmss::run();
  EXPECT_TRUE(i == n_reactor * n_round);
  pmss::deinit_service();
}
