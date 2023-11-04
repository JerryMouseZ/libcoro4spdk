#include "service.hpp"
#include "spdk/env.h"
#include "task.hpp"
#include <catch2/catch_all.hpp>
#include <catch2/catch_test_macros.hpp>
const char *json_file = "bdev.json";
const char *bdev_dev = "Malloc0";
task<int> simple_write_read() {
  // 不用担心，因为这个协程肯定是执行在spdk线程里的
  char *dma_buf = (char *)spdk_dma_zmalloc(4096, 4096, nullptr);
  strcpy(dma_buf, "hello world");
  int rc = co_await g_service->write(dma_buf, 4096, 0);
  REQUIRE(rc == 0);
  memset(dma_buf, 0, 4096);
  rc = co_await g_service->read(dma_buf, 4096, 0);
  REQUIRE(rc == 0);
  REQUIRE(strcmp(dma_buf, "hello world") == 0);
  co_return 0;
}

TEST_CASE("simple_io", "simple_write_read") {
  init_service(1, json_file, bdev_dev);
  g_service->run(simple_write_read());
  deinit_service();
}
