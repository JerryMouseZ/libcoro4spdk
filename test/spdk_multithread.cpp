#include "service.hpp"
#include "spdk/env.h"
#include "task.hpp"
#include <cstdio>
#include <gtest/gtest.h>
#include "common.hpp"
task<int> simple_write_read(int i) {
  // 不用担心，因为这个协程肯定是执行在spdk线程里的
  fprintf(stderr, "simple write read %d begin\n", i);
  char* dma_buf = (char*)spdk_dma_zmalloc(4096, 4096, nullptr);
  strcpy(dma_buf, "hello world");
  int rc = co_await pmss::write(dma_buf, 4096, 0);
  EXPECT_TRUE(rc == 0);
  memset(dma_buf, 0, 4096);
  rc = co_await pmss::read(dma_buf, 4096, 0);
  EXPECT_TRUE(rc == 0);
  EXPECT_TRUE(strcmp(dma_buf, "hello world") == 0);
  fprintf(stderr, "simple write read %d done\n", i);
  co_return 0;
}

TEST(multithread_io, multiple_write_read) {
  pmss::init_service(8, json_file, bdev_dev);
  for (int i = 0; i < 8; ++i)
    pmss::add_task(simple_write_read(i));
  pmss::run();
  pmss::deinit_service();
}
