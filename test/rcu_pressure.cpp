#include "rcu.hpp"
#include "spinlock.hpp"
#include "service.hpp"
#include "spdk/env.h"
#include "task.hpp"
#include <catch2/catch_all.hpp>
#include <catch2/catch_test_macros.hpp>
#include <atomic>
#include <cstdio>

const char *json_file = "bdev.json";
const char *bdev_dev = "Malloc0";

struct Foo {
    int a = 0;
    int b = 0;
    int c = 0;
};

Foo *gp = new Foo();

int n_round = 100000;

task<int> reader(int idx) {
    for (int i=0; i<n_round; i++) {
        rcu::rcu_read_lock();

        Foo* p = gp;
        REQUIRE(p->a >= 0);
        REQUIRE(p->a < 55);
        REQUIRE(p->a == p->b);
        REQUIRE(p->b == p->c);
        printf("%d: %d %d %d\n", idx, p->a, p->b, p->c);

        rcu::rcu_read_unlock();
    }
    co_return 0;
}

async_simple::coro::SpinLock spinlock;

task<int> writer(int idx) {
    for (int i=0; i<n_round; i++) {
        Foo *p = new Foo();
        p->a = idx;
        p->b = idx;
        p->c = idx;

        co_await spinlock.coLock();

        Foo *q = gp;
        rcu_assign_pointer(gp, p);

        co_await rcu::rcu_sync_run();

        printf("%d: %d\n", idx, i);

        free(q);
        q = nullptr;

        spinlock.unlock();
    }
    co_return 0;
}

TEST_CASE("rcu_pressure_test") {
    init_service(8, json_file, bdev_dev);
    for (int i=0; i<50; i++) {
        g_service->add_task(reader(i));
    }
    for (int i=50; i<55; i++) {
        g_service->add_task(writer(i));
    }
    g_service->run();
    REQUIRE(gp != nullptr);
    deinit_service();
}