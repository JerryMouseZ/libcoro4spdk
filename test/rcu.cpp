#include "rcu.hpp"
#include "service.hpp"
#include "spdk/env.h"
#include "spinlock.hpp"
#include "task.hpp"
#include <atomic>
#include <catch2/catch_all.hpp>
#include <catch2/catch_test_macros.hpp>
#include <cstdio>

const char* json_file = "bdev.json";
const char* bdev_dev = "Malloc0";

struct Foo {
  int a = 0;
  int b = 0;
  int c = 0;
};

Foo* gp = new Foo();

task<int> reader(int i) {
  rcu::rcu_read_lock();
  Foo* p = gp;

  assert(p->a == 0 || p->a == 2);
  REQUIRE(p->a == p->b);
  REQUIRE(p->b == p->c);

  rcu::rcu_read_unlock();

  co_return 0;
}

async_simple::coro::SpinLock spinlock;

task<int> writer(int i) {
  Foo* p = new Foo();
  p->a = i;
  p->b = i;
  p->c = i;

  Foo* q = gp;
  rcu_assign_pointer(gp, p);
  co_await rcu::rcu_sync_run();
  free(q);
  q = nullptr;
  co_return 0;
}

TEST_CASE("rcu_test") {
  init_service(3, json_file, bdev_dev);
  g_service->add_task(reader(1));
  g_service->add_task(writer(2));
  g_service->add_task(reader(3));
  g_service->run();
  REQUIRE(gp != NULL);
  REQUIRE(gp->a == 2);
  REQUIRE(gp->b == 2);
  REQUIRE(gp->c == 2);
  deinit_service();
}