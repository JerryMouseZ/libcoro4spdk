#include "mutex.hpp"
#include "rcu.hpp"
#include "spinlock.hpp"
#include "task.hpp"
#include <cstdio>
#include <gtest/gtest.h>
#include "common.hpp"

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
    EXPECT_TRUE(p->a == p->b);
    EXPECT_TRUE(p->b == p->c);
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
    EXPECT_TRUE(p->a == p->b);
    EXPECT_TRUE(p->b == p->c);
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
    EXPECT_TRUE(p->a == p->b);
    EXPECT_TRUE(p->b == p->c);
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

TEST(rcu_schedule, rcu_schedule) {
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
  EXPECT_TRUE(gp != NULL);
  pmss::deinit_service();
}
