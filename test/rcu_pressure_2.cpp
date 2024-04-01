#include "rcu.hpp"
#include "mutex.hpp"
#include "task.hpp"
#include <gtest/gtest.h>
#include <cstdio>
#include <unistd.h>
#include "common.hpp"
#include "schedule.hpp"

/* A improved version of rcu_pressure.cpp*/

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

Foo* gp = new Foo(n_reader - 1);

task<int> reader(int idx) {
  for (int i = 0; i < n_round; i++) {
    pmss::rcu::rcu_read_lock();

    Foo* p = (Foo*)gp;

    EXPECT_TRUE(gp != nullptr);
    EXPECT_TRUE(p != nullptr);
    EXPECT_TRUE(p->a >= n_reader - 1);
    EXPECT_TRUE(p->a < n_reader + n_writer);
    // EXPECT_TRUE(p->b >= n_reader-1);
    // EXPECT_TRUE(p->b < n_reader+n_writer);
    // EXPECT_TRUE(p->c >= n_reader-1);
    // EXPECT_TRUE(p->c < n_reader+n_writer);
    EXPECT_TRUE(p->a == p->b);
    EXPECT_TRUE(p->b == p->c);

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

    co_await pmss::rcu::synchronize_rcu();

    delete q;
    mutex.unlock();
  }
  co_return 0;
}

TEST(rcu_pressure_test, rcu_pressure_test_2) {
  printf("iter: %d, reader: %d, writer: %d\n", n_round, n_reader, n_writer);
  pmss::init_service(16, json_file, bdev_dev);
  for (int i = 0; i < n_reader; i++) {
    pmss::add_task(reader(i));
  }
  for (int i = n_reader; i < n_reader + n_writer; i++) {
    pmss::add_task(writer(i));
  }
  pmss::run();
  EXPECT_TRUE(gp != nullptr);
  EXPECT_TRUE(gp->a >= n_reader - 1);
  EXPECT_TRUE(gp->a < n_reader + n_writer);
  EXPECT_TRUE(gp->a == gp->b);
  EXPECT_TRUE(gp->b == gp->c);
  pmss::deinit_service();
}
