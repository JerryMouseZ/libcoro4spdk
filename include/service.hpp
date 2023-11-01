#ifndef SERVICE_H
#define SERVICE_H
#include "module.hpp"
#include "spdk/cpuset.h"
#include "spdk/event.h"
#include "task.hpp"
#include <algorithm>
#include <barrier>
#include <cassert>
#include <coroutine>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <list>
#include <pthread.h>
#include <vector>

#ifndef NDEBUG
#define DEBUG_PRINTF(...)                                                      \
  do {                                                                         \
    fprintf(stderr, "[%s:%d]: ", __FILE__, __LINE__);                          \
    fprintf(stderr, __VA_ARGS__);                                              \
  } while (0);
#else
#define DEBUG_PRINTF(...) void(0)
#endif // !NDEBUG

void service_init(void *args);
struct spdk_service {
  struct reactor_data {
    spdk_thread *thread;
    spdk_io_channel *ch;
    // maybe need dma buffer
    // 这里如果用库分配的dma buffer，那么不是得需要一次copy了，这是个问题
  };

  std::vector<reactor_data> rds;
  std::vector<task<int>> tasks;
  int num_threads;
  char cpumask[4];
  spdk_app_opts opts;
  int current_core = 0;
  std::barrier<> exit_barrier;
  spdk_bdev_desc *desc;
  const char *bdev_name;

  spdk_service(int num_threads, const char *json_file, const char *bdev_name)
      : rds(num_threads), bdev_name(bdev_name), exit_barrier(num_threads) {
    this->num_threads = num_threads;
    spdk_app_opts_init(&opts, sizeof(opts));
    opts.name = "myapp";
    opts.json_config_file = json_file;
    spdk_cpuset mask;
    spdk_cpuset_zero(&mask);
    for (int i = 0; i < num_threads; ++i) {
      spdk_cpuset_set_cpu(&mask, i, true);
    }
    strcpy(cpumask, spdk_cpuset_fmt(&mask));
    opts.reactor_mask = cpumask;
    // 由于app_start调用了之后就阻塞住了，因此这里不能start，而是要等协程都开始运行之后才可以
  }

  struct result {
    std::coroutine_handle<> coro;
    int res;
  };

  struct service_awaiter {
    result res;
    bool await_ready() { return false; }
    auto await_suspend(std::coroutine_handle<> coro) { res.coro = coro; }
    auto await_resume() { return res.res; }
    explicit service_awaiter(){};
  };

  // buf must be dma buffer
  service_awaiter read(void *buf, int len, size_t offset) {}

  service_awaiter write(void *buf, int len, size_t offset) {}

  // 本来不应该有这种用法的，不过既然有直接在当前的reactor上运行是不是也可以，
  // 这种不能产生运行结果，所以不进入tasks中
  void add_task(task<void> &&t) { t.start(); }

  void run(task<int> &&t) {
    tasks.emplace_back(std::move(t));
    spdk_app_start(&opts, service_init, this);
  }

  template <typename... Args> void run(task<int> &&t, Args... args) {
    tasks.emplace_back(std::move(t));
    run(args...);
  }
};
#endif // !DEBUG
