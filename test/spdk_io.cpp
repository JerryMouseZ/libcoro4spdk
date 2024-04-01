#include "service.hpp"
#include "spdk/env.h"
#include "task.hpp"
#include <cstdio>
#include <gtest/gtest.h>
#include "common.hpp"

task<int> simple_write_read() {
  // 不用担心，因为这个协程肯定是执行在spdk线程里的
  char* dma_buf = (char*)spdk_dma_zmalloc(4096, 4096, nullptr);
  strcpy(dma_buf, "hello world");
  int rc = co_await pmss::write(dma_buf, 4096, 0);
  EXPECT_TRUE(rc == 0);
  memset(dma_buf, 0, 4096);
  rc = co_await pmss::read(dma_buf, 4096, 0);
  EXPECT_TRUE(rc == 0);
  EXPECT_TRUE(strcmp(dma_buf, "hello world") == 0);
  fprintf(stderr, "simple write read done\n");
  co_return 0;
}

TEST(simple_io, simple_write_read) {
  pmss::init_service(1, json_file, bdev_dev);
  pmss::run(simple_write_read());
  pmss::deinit_service();
}
