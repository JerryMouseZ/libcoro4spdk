#include "service.hpp"
#include "task.hpp"
#include <catch2/catch_all.hpp>
#include <catch2/catch_test_macros.hpp>

const char *json_file = "bdev.json";
const char *bdev_dev = "Malloc0";
const int n_reactor = 8;
const int n_task = 10;
const int n_round = 100000;

task<int> task_int_test(int task_id) {
  for (int i = 0; i < n_round; i++) {
    /* std::cout << task_id << "-1" << std::endl; */
    co_await YieldAwaiter();
    /* std::cout << task_id << "-2" << std::endl; */
  }
  co_return 0;
}

TEST_CASE("yield_test", "task int") {
  init_service(n_reactor, json_file, bdev_dev);
  for (int i = 0; i < n_reactor * n_task; i++) {
    g_service->add_task(task_int_test(i));
  }
  g_service->run();
  deinit_service();
}
