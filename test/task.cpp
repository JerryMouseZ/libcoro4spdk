#include "task.hpp"
#include <coroutine>
#include <cstdio>
#include <unistd.h>
#include <gtest/gtest.h>
#include "common.hpp"

struct wakable {
  bool await_ready() const noexcept {
    printf("await ready\n");
    return false;
  }
  void await_suspend(std::coroutine_handle<> handle) {
    printf("await suspend: %p\n", handle.address());
    _h = handle;
  }
  void await_resume() {}
  std::coroutine_handle<> _h;
  void wakeup() { _h.resume(); }
};

task<int> async_func(wakable& w) {
  printf("async_func begin\n");
  printf("before suspend\n");
  co_await w;
  printf("async_func done\n");
  co_return 1;
}

TEST(resume_from_subroutine, subroutine) {
  wakable w{};
  auto t = async_func(w);
  t.start();
  printf("%pvs%p\n", t._h.address(), w._h.address());
  w.wakeup();
  EXPECT_TRUE(t._h.promise()._value.value() == 1);
}

task<int> empty() {
  co_return 2;
}
task<int> async_func2() {
  int ret = co_await empty();
  co_return ret;
}

TEST(resume_from_empty, empty) {
  auto t = async_func2();
  t.start();
  EXPECT_TRUE(t.get().value() == 2);
}

task<int> async_wrapper(wakable& w) {
  int ret = co_await async_func(w);
  co_return ret;
}

TEST(subroutine_wrapper, subroutine) {
  wakable w;
  auto t = async_wrapper(w);
  t.start();
  w.wakeup();
  EXPECT_TRUE(t.get().value() == 1);
}

task<void> empty_loop() {
  for (int i = 0; i < 100000; ++i) {
    co_await empty();
  }
}
TEST(empty_loop, subroutine) {
  auto t = empty_loop();
  t.start();
}
