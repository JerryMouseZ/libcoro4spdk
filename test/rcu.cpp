#include "rcu.hpp"
#include "task.hpp"
#include "schedule.hpp"
#include <gtest/gtest.h>
#include "common.hpp"
#include <cstdio>

struct Foo {
  int a = 0;
  int b = 0;
  int c = 0;
};

Foo* gp = new Foo();

task<int> reader(int i) {
  pmss::rcu::rcu_read_lock();
  Foo* p = gp;

  assert(p->a == 0 || p->a == 2);
  EXPECT_TRUE(p->a == p->b);
  EXPECT_TRUE(p->b == p->c);

  pmss::rcu::rcu_read_unlock();

  co_return 0;
}

task<int> writer(int i) {
  Foo* p = new Foo();
  p->a = i;
  p->b = i;
  p->c = i;

  Foo* q = gp;
  pmss::rcu::rcu_assign_pointer(gp, p);

  co_await pmss::rcu::synchronize_rcu();

  delete q;
  co_return 0;
}

TEST(rcu_test, rcu_test) {
  pmss::init_service(3, json_file, bdev_dev);
  pmss::add_task(reader(1));
  pmss::add_task(writer(2));
  pmss::add_task(reader(3));
  pmss::run();
  EXPECT_TRUE(gp != NULL);
  EXPECT_TRUE(gp->a == 2);
  EXPECT_TRUE(gp->b == 2);
  EXPECT_TRUE(gp->c == 2);
  pmss::deinit_service();
}
