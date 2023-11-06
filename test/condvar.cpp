#include "spinlock.hpp"
#include "conditionvariable.hpp"
#include "service.hpp"
#include "spdk/env.h"
#include "task.hpp"
#include <catch2/catch_all.hpp>
#include <catch2/catch_test_macros.hpp>

const char *json_file = "bdev.json";
const char *bdev_dev = "Malloc0";

async_simple::coro::SpinLock spinlock;
async_simple::coro::ConditionVariable<async_simple::coro::SpinLock> cv;
int value = 0;

task<int> producer_1() {
    co_await spinlock.coLock();
    value++;
    cv.notify();
    spinlock.unlock();
    co_return 0;
}

task<int> consumer_1() {
    co_await spinlock.coLock();
    co_await cv.wait(spinlock, [&]{ return value > 0; });
    spinlock.unlock();
    assert(value > 0);
    co_return 0;
}

TEST_CASE("condvar test 1", "subtest 1") {
    init_service(2, json_file, bdev_dev);
    g_service->add_task(producer_1());
    g_service->add_task(consumer_1());
    g_service->run();
    deinit_service();
}

async_simple::coro::Notifier notifier;  // Notifier = ConditionVariable<void>
bool ready = false;

task<int> producer_2() {
    ready = true;
    notifier.notify();
    co_return 0;
}

task<int> consumer_2() {
    co_await notifier.wait();
    assert(ready);
    co_return 0;
}

TEST_CASE("condvar test 2", "subtest 2") {
    init_service(2, json_file, bdev_dev);
    g_service->add_task(producer_2());
    g_service->add_task(consumer_2());
    g_service->run();
    deinit_service();
}