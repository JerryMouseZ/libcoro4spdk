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

Foo* gp = new Foo{0, 1, 2};

int n_round = 100000;
int n_reader = 32;
int n_writer = 4;

task<int> reader(int idx) {
  for (int i = 0; i < n_round; i++) {
    pmss::rcu::rcu_read_lock();

    Foo* p = pmss::rcu::rcu_dereference(gp);
    REQUIRE(p->a + 1 == p->b);
    REQUIRE(p->b + 1 == p->c);

    pmss::rcu::rcu_read_unlock();
  }
  fprintf(stderr, "reader %d complete!\n", idx);
  co_return 0;
}

async_simple::coro::Mutex mutex;

task<int> writer(int idx) {
  for (int i = 0; i < n_round; i++) {
    co_await mutex.coLock();
    Foo* p = new Foo();
    p->a = idx;
    p->b = idx + 1;
    p->c = idx + 2;

    Foo* oldgp = (Foo*)gp;
    pmss::rcu::rcu_assign_pointer(gp, p);
    co_await pmss::rcu::synchronize_rcu();

    delete oldgp;
    mutex.unlock();
  }
  fprintf(stderr, "writer %d complete!\n", idx);
  co_return 0;
}

TEST_CASE("rcu_pressure_test") {
  pmss::init_service(8, json_file, bdev_dev);
  for (int i = 0; i < n_reader; i++) {
    pmss::add_task(reader(i));
  }
  for (int i = n_reader; i < n_reader + n_writer; i++) {
    pmss::add_task(writer(i));
  }
  pmss::run();
  REQUIRE(gp != nullptr);
  pmss::deinit_service();
}
