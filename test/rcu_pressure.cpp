#include "rcu.hpp"
#include "service.hpp"
#include "spdk/env.h"
#include "mutex.hpp"
#include "task.hpp"
#include <atomic>
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <catch2/catch_all.hpp>
#include <catch2/catch_test_macros.hpp>

const char* json_file = "bdev.json";
const char* bdev_dev = "Malloc0";

struct Foo {
  int a = 0;
  int b = 0;
  int c = 0;
};

Foo* gp = new Foo();

int n_round = 100000;
int n_reader = 100;
int n_writer = 10;

task<int> reader(int idx) {
  for (int i = 0; i < n_round; i++) {
    pmss::rcu::rcu_read_lock();
    Foo* p = (Foo*)gp;
    REQUIRE(p->a == p->b);
    REQUIRE(p->b == p->c);

    // printf("%d: %d %d %d\n", idx, p->a, p->b, p->c);

    pmss::rcu::rcu_read_unlock();
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

    Foo* q = (Foo*)gp;
    pmss::rcu::rcu_assign_pointer(gp, p);

    co_await pmss::rcu::rcu_sync_run();

    // printf("%d: %d\n", idx, i);

    delete q;

    mutex.unlock();
  }
  co_return 0;
}

TEST_CASE("rcu_pressure_test") {
// int main(int argc, char* argv[]) {
//   assert(argc == 4);
//   n_round = atoi(argv[1]);
//   n_reader = atoi(argv[2]);
//   n_writer = atoi(argv[3]);

  printf("iter: %d, reader: %d, writer: %d\n", n_round, n_reader, n_writer);

  pmss::init_service(16, json_file, bdev_dev);
  for (int i = 0; i < n_reader; i++) {
    pmss::add_task(reader(i));
  }
  for (int i = n_reader; i < n_reader + n_writer; i++) {
    pmss::add_task(writer(i));
  }

  pmss::run();

  // assert(gp != nullptr);
  // assert(gp->a == 0 || gp->a >= n_reader);
  // assert(gp->a < n_reader+n_writer);
  // assert(gp->a == gp->b);
  // assert(gp->b == gp->c);

  REQUIRE(gp != nullptr);
  
  REQUIRE(gp->a == gp->b);
  REQUIRE(gp->b == gp->c);

  pmss::deinit_service();
}
