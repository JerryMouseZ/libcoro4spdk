#include "rcu.hpp"
#include "service.hpp"
#include "spdk/env.h"
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
  pmss::rcu::rcu_read_lock();
  Foo* p = gp;

  assert(p->a == 0 || p->a == 2);
  REQUIRE(p->a == p->b);
  REQUIRE(p->b == p->c);

  pmss::rcu::rcu_read_unlock();
  fprintf(stderr, "reader %d complete\n", i);

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
  fprintf(stderr, "writer %d complete\n", i);
  co_return 0;
}

TEST_CASE("rcu_test") {
  pmss::init_service(3, json_file, bdev_dev);
  pmss::add_task(reader(1));
  pmss::add_task(writer(2));
  pmss::add_task(reader(3));
  pmss::run();
  REQUIRE(gp != NULL);
  REQUIRE(gp->a == 2);
  REQUIRE(gp->b == 2);
  REQUIRE(gp->c == 2);
  pmss::deinit_service();
}
