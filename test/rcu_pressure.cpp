#include "rcu.hpp"
#include "service.hpp"
#include "spdk/env.h"
#include "mutex.hpp"
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

int n_round = 100000;
int n_reader = 1000;
int n_writer = 1;


task<int> reader(int idx) {
  for (int i = 0; i < n_round; i++) {
    rcu::rcu_read_lock();

    Foo* p = gp;
    REQUIRE(p->a >= 0);
    REQUIRE(p->a < 55);
    REQUIRE(p->a == p->b);
    REQUIRE(p->b == p->c);
    // printf("%d: %d %d %d\n", idx, p->a, p->b, p->c);

    rcu::rcu_read_unlock();
  }
  co_return 0;
}

async_simple::coro::Mutex mutex;

task<int> writer(int idx) {
  for (int i = 0; i < n_round; i++) {
    Foo* p = new Foo();
    p->a = idx;
    p->b = idx;
    p->c = idx;

    co_await mutex.coLock();

    Foo* q = gp;
    rcu::rcu_assign_pointer(&gp, &p);

    co_await rcu::rcu_sync_run();

    // printf("%d: %d\n", idx, i);

    free(q);

    mutex.unlock();
  }
  co_return 0;
}

TEST_CASE("rcu_pressure_test") {
  pmss::init_service(16, json_file, bdev_dev);
  for (int i = 0; i < n_reader; i++) {
    pmss::add_task(reader(i));
  }
  for (int i = n_reader; i < n_reader+n_writer; i++) {
    pmss::add_task(writer(i));
  }
  pmss::run();
  REQUIRE(gp != nullptr);
  REQUIRE(gp->a >= 0);
  REQUIRE(gp->a < 55);
  REQUIRE(gp->a == gp->b);
  REQUIRE(gp->b == gp->c);
  pmss::deinit_service();
}
