#include "mutex.hpp"
#include "service.hpp"
#include "spdk/env.h"
#include "task.hpp"
#include <catch2/catch_all.hpp>
#include <catch2/catch_test_macros.hpp>
#include <cstdio>
#include <vector>
const char *json_file = "bdev.json";
const char *bdev_dev = "Malloc0";

int i = 0;
async_simple::coro::Mutex mutex;
task<int> lock_test() {
  for (int j = 0; j < 100000; ++j) {
    co_await mutex.coLock();
    i++;
    mutex.unlock();
  }
  co_return 0;
}

TEST_CASE("simple_lock", "simple_lock") {
  init_service(8, json_file, bdev_dev);
  for (int i = 0; i < 8; ++i) {
    g_service->add_task(lock_test());
  }
  g_service->run();
  REQUIRE(i == 8 * 100000);
  deinit_service();
}
