#include "schedule.hpp"
#include "task.hpp"
#include <gtest/gtest.h>
#include "common.hpp"
const int n_reactor = 8;
const int n_task = 10;
const int n_round = 100000;

task<int> task_int_test(int task_id) {
  for (int i = 0; i < n_round; i++) {
    co_await yield();
  }
  co_return 0;
}

TEST(yield_test, task_int) {
  pmss::init_service(n_reactor, json_file, bdev_dev);
  for (int i = 0; i < n_reactor * n_task; i++) {
    pmss::add_task(task_int_test(i));
  }
  pmss::run();
  pmss::deinit_service();
}
