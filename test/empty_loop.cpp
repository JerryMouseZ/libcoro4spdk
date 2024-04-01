#include "task.hpp"
#include <gtest/gtest.h>

task<int> empty() {
  co_return 2;
}
task<int> empty_loop() {
  int r = 0;
  for (int i = 0; i < 100000; ++i) {
    r += co_await empty();
  }
  co_return r;
}

TEST(empty_test, empty_loop) {
  auto t = empty_loop();
  t.start();
  EXPECT_TRUE(t.get().value() == 100000 * 2);
}
