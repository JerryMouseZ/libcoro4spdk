#include "rcu.hpp"
#include "spinlock.hpp"
#include "task.hpp"
#include <gtest/gtest.h>
#include "common.hpp"

struct Foo {
  int a = 0;
  int b = 0;
  int c = 0;
};

Foo* gp = new Foo();

int n_round = 100000;
int n_reader = 32;
int n_writer = 4;

task<int> reader(int idx) {
  for (int i = 0; i < n_round; i++) {
    pmss::rcu::rcu_read_lock();

    Foo* p = pmss::rcu::rcu_dereference(gp);
    EXPECT_TRUE(p->a == p->b);
    EXPECT_TRUE(p->b == p->c);

    /* std::atomic_thread_fence(std::memory_order_seq_cst); */
    pmss::rcu::rcu_read_unlock();
  }
  co_return 0;
}

async_simple::coro::SpinLock mutex;

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

TEST(rcu_pressure_test, rcu_pressure_test) {
  pmss::init_service(32, json_file, bdev_dev);
  for (int i = 0; i < n_reader; i++) {
    pmss::add_task(reader(i));
  }
  for (int i = n_reader; i < n_reader + n_writer; i++) {
    pmss::add_task(writer(i));
  }
  pmss::run();
  EXPECT_TRUE(gp != nullptr);
  EXPECT_TRUE(gp->a == gp->b);
  EXPECT_TRUE(gp->b == gp->c);
  pmss::deinit_service();
}
