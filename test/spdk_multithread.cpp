#include "service.hpp"
#include "spdk/env.h"
#include "task.hpp"
#include <catch2/catch_all.hpp>
#include <catch2/catch_test_macros.hpp>
#include <cstdio>
#include <vector>
const char* json_file = "bdev.json";
const char* bdev_dev = "Malloc0";
task<int> simple_write_read(int i) {
  // 不用担心，因为这个协程肯定是执行在spdk线程里的
  fprintf(stderr, "simple write read %d begin\n", i);
  char* dma_buf = (char*)spdk_dma_zmalloc(4096, 4096, nullptr);
  strcpy(dma_buf, "hello world");
  int rc = co_await pmss::write(dma_buf, 4096, 0);
  REQUIRE(rc == 0);
  memset(dma_buf, 0, 4096);
  rc = co_await pmss::read(dma_buf, 4096, 0);
  REQUIRE(rc == 0);
  REQUIRE(strcmp(dma_buf, "hello world") == 0);
  fprintf(stderr, "simple write read %d done\n", i);
  co_return 0;
}

TEST_CASE("multithread_io", "multiple_write_read") {
  pmss::init_service(8, json_file, bdev_dev);
  for (int i = 0; i < 8; ++i)
    pmss::add_task(simple_write_read(i));
  pmss::run();
  pmss::deinit_service();
}

/* TEST_CASE("multithread_io_more", "multiple_write_read") { */
/*   init_service(8, json_file, bdev_dev); */
/*   for (int i = 0; i < 8; ++i) */
/*     pmss::add_task(simple_write_read(i)); */
/*   pmss::run(); */
/*   deinit_service(); */
/* } */
