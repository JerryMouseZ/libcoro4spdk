#include "mutex.hpp"
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
int n_round = 10000;
int n_reader = 32;
int n_writer = 4;

async_simple::coro::SpinLock spinlock;
task<int> spinlock_reader(int i) {
  for (int i = 0; i < n_round; i++) {
    pmss::rcu::rcu_read_lock();
    Foo* p = pmss::rcu::rcu_dereference(gp);
    REQUIRE(p->a == p->b);
    REQUIRE(p->b == p->c);
    pmss::rcu::rcu_read_unlock();

    if (n_round % 1024 == 0) {
      co_await spinlock.coLock();
      spinlock.unlock();
    }
  }
  co_return 0;
}

task<int> rcu_sync_reader(int i) {
  for (int i = 0; i < n_round; i++) {
    pmss::rcu::rcu_read_lock();
    Foo* p = pmss::rcu::rcu_dereference(gp);
    REQUIRE(p->a == p->b);
    REQUIRE(p->b == p->c);
    pmss::rcu::rcu_read_unlock();

    if (n_round % 1024 == 0)
      co_await pmss::rcu::synchronize_rcu();
  }
  co_return 0;
}

async_simple::coro::Mutex mutex;
task<int> mutex_reader(int i) {
  for (int i = 0; i < n_round; i++) {
    pmss::rcu::rcu_read_lock();
    Foo* p = pmss::rcu::rcu_dereference(gp);
    REQUIRE(p->a == p->b);
    REQUIRE(p->b == p->c);
    pmss::rcu::rcu_read_unlock();

    if (n_round % 1024 == 0) {
      co_await mutex.coLock();
      mutex.unlock();
    }
  }
  co_return 0;
}

task<int> writer(int idx) {
  for (int i = 0; i < n_round; i++) {
    Foo* p = new Foo();
    p->a = idx;
    p->b = idx;
    p->c = idx;
    co_await mutex.coLock();
    Foo* q = (Foo*)gp;
    pmss::rcu::rcu_assign_pointer(gp, p);
    co_await pmss::rcu::synchronize_rcu();
    delete q;
    mutex.unlock();
  }
  co_return 0;
}

TEST_CASE("rcu_schedule") {
  pmss::init_service(3, json_file, bdev_dev);
  for (int i = 0; i < n_reader; i++) {
    pmss::add_task(spinlock_reader(i));
    pmss::add_task(rcu_sync_reader(i));
    pmss::add_task(mutex_reader(i));
  }

  for (int i = n_reader; i < n_reader + n_writer; i++) {
    pmss::add_task(writer(i));
  }

  pmss::run();
  REQUIRE(gp != NULL);
  pmss::deinit_service();
}
