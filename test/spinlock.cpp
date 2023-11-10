#include "spinlock.hpp"
#include "service.hpp"
#include "spdk/env.h"
#include "task.hpp"
#include <catch2/catch_all.hpp>
#include <catch2/catch_test_macros.hpp>
#include <cstdio>

const char *json_file = "bdev.json";
const char *bdev_dev = "Malloc0";
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

TEST_CASE("spinlock", "simple_lock") {
  init_service(n_reactor, json_file, bdev_dev);
  for (int i = 0; i < n_reactor; i++) {
    g_service->add_task(spinlock_test());
  }
  g_service->run();
  REQUIRE(i == n_reactor * n_round);
  deinit_service();
}
