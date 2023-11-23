#include "rcu.hpp"
#include "service.hpp"
#include "spdk/env.h"
#include "mutex.hpp"
#include "task.hpp"
#include <atomic>
#include <catch2/catch_all.hpp>
#include <catch2/catch_test_macros.hpp>
#include <cstdio>
#include <unistd.h>

/* A improved version of rcu_pressure.cpp*/

const char* json_file = "bdev.json";
const char* bdev_dev = "Malloc0";

struct Foo {
  int a;
  int b;
  int c;
  Foo(int v) {
    a = v;
    b = v;
    c = v;
  }
};

int n_round = 100000;
int n_reader = 100;
int n_writer = 10;

Foo* gp = new Foo(n_reader-1);

task<int> reader(int idx) {
  for (int i = 0; i < n_round; i++) {
    pmss::rcu::rcu_read_lock();

    Foo* p = (Foo*)gp;

    REQUIRE(gp != nullptr);
    REQUIRE(p != nullptr);
    REQUIRE(p->a >= n_reader-1);
    REQUIRE(p->a < n_reader+n_writer);
    // REQUIRE(p->b >= n_reader-1);
    // REQUIRE(p->b < n_reader+n_writer);
    // REQUIRE(p->c >= n_reader-1);
    // REQUIRE(p->c < n_reader+n_writer);
    REQUIRE(p->a == p->b);
    REQUIRE(p->b == p->c);

    pmss::rcu::rcu_read_unlock();
  }
  co_return 0;
}

async_simple::coro::Mutex mutex;

task<int> writer(int idx) {
  for (int i = 0; i < n_round; i++) {
    Foo* p = new Foo(idx);

    co_await mutex.coLock();

    Foo* q = (Foo*)gp;
    pmss::rcu::rcu_assign_pointer(gp, p);

    co_await pmss::rcu::rcu_sync_run();

    delete q;
    mutex.unlock();
  }
  co_return 0;
}

TEST_CASE("rcu_pressure_test") {
  printf("iter: %d, reader: %d, writer: %d\n", n_round, n_reader, n_writer);
  pmss::init_service(16, json_file, bdev_dev);
  for (int i = 0; i < n_reader; i++) {
    pmss::add_task(reader(i));
  }
  for (int i = n_reader; i < n_reader + n_writer; i++) {
    pmss::add_task(writer(i));
  }
  pmss::run();
  REQUIRE(gp != nullptr);
  REQUIRE(gp->a >= n_reader-1);
  REQUIRE(gp->a < n_reader+n_writer);
  REQUIRE(gp->a == gp->b);
  REQUIRE(gp->b == gp->c);
  pmss::deinit_service();
}
