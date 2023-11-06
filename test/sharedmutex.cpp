#include "sharedmutex.hpp"
#include "service.hpp"
#include "spdk/env.h"
#include "task.hpp"
#include <catch2/catch_all.hpp>
#include <catch2/catch_test_macros.hpp>

const char *json_file = "bdev.json";
const char *bdev_dev = "Malloc0";

async_simple::coro::SharedMutex rw_lock;
int value = 0;

task<int> reader() {
    co_await rw_lock.coLockShared();
    std::cout << value << std::endl;
    co_await rw_lock.unlockShared();
    co_return 0;
}

task<int> writer() {
    co_await rw_lock.coLock();
    value++;
    co_await rw_lock.unlock();
    co_return 0;
}

TEST_CASE("read/write lock test", "simple test") {
    init_service(2, json_file, bdev_dev);
    g_service->add_task(reader());
    g_service->add_task(reader());
    g_service->add_task(writer());
    g_service->add_task(writer());
    g_service->add_task(reader());
    g_service->add_task(reader());
    g_service->run();
    deinit_service();
}