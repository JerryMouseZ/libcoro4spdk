#ifndef SERVICE_H
#define SERVICE_H
#include "module.hpp"
#include "spdk/accel.h"
#include "spdk/bdev.h"
#include "spdk/cpuset.h"
#include "spdk/env.h"
#include "spdk/event.h"
#include "task.hpp"
#include <algorithm>
#include <barrier>
#include <cassert>
#include <coroutine>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iostream>
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

void spdk_io_complete_cb(struct spdk_bdev_io *bdev_io, bool success,
                         void *cb_arg);

void spdk_retry_read(void *args);

void spdk_retry_write(void *args);

struct spdk_service {
  struct result {
    std::coroutine_handle<> coro;
    int res;
  };

  struct retry_context {
    spdk_bdev_desc *desc;
    spdk_io_channel *ch;
    void *buf;
    int len;
    size_t offset;
    result *res;
  };

  // cacheline aligned will be better to prevent races
  struct reactor_data {
    spdk_thread *thread;
    spdk_io_channel *ch;
    spdk_bdev_io_wait_entry bdev_io_wait;
    retry_context context;
    // maybe need dma buffer
    // 这里如果用库分配的dma buffer，那么不是得需要一次copy了，这是个问题
  };

  const char *device_name;
  std::vector<reactor_data> rds;
  std::vector<task<int>> tasks;
  int num_threads;
  char cpumask[8] = "0x";
  spdk_app_opts opts;
  int current_core = 0;
  std::barrier<> exit_barrier;
  spdk_bdev_desc *desc;
  spdk_bdev *bdev;

  spdk_service(int num_threads, const char *json_file, const char *bdev_name)
      : rds(num_threads), exit_barrier(num_threads), device_name(bdev_name) {
    /* memcpy(device_name, bdev_name, 16); */
    this->num_threads = num_threads;
    spdk_app_opts_init(&opts, sizeof(opts));
    opts.name = "spdk_service";
    opts.json_config_file = json_file;
    spdk_cpuset mask;
    spdk_cpuset_zero(&mask);
    for (int i = 0; i < num_threads; ++i) {
      spdk_cpuset_set_cpu(&mask, i, true);
    }
    strcpy(cpumask + 2, spdk_cpuset_fmt(&mask));
    opts.reactor_mask = cpumask;
    // 由于app_start调用了之后就阻塞住了，因此这里不能start，而是要等协程都开始运行之后才可以
  }

  struct service_awaiter {
    result res;
    bool ready;
    bool await_ready() { return ready; }
    auto await_suspend(std::coroutine_handle<> coro) { res.coro = coro; }
    auto await_resume() { return res.res; }
    explicit service_awaiter() : ready(false) {}
  };

  // buf must be dma buffer
  service_awaiter read(void *buf, int len, size_t offset) {
    int current_core = spdk_env_get_current_core();
    service_awaiter awaiter{};
    int rc = spdk_bdev_read(desc, rds[current_core].ch, buf, offset, len,
                            spdk_io_complete_cb, &awaiter.res);
    if (rc == -ENOMEM) {
      // retry queue io
      rds[current_core].bdev_io_wait.bdev = spdk_bdev_desc_get_bdev(desc);
      rds[current_core].bdev_io_wait.cb_fn = spdk_retry_read;
      rds[current_core].context.buf = buf;
      rds[current_core].context.len = len;
      rds[current_core].context.offset = offset;
      rds[current_core].context.res = &awaiter.res;
      rds[current_core].bdev_io_wait.cb_arg = &rds[current_core].context;
      spdk_bdev_queue_io_wait(bdev, rds[current_core].ch,
                              &rds[current_core].bdev_io_wait);
    } else if (rc) {
      // don't use awaiter
      awaiter.ready = true;
    }

    return awaiter;
  }

  service_awaiter write(void *buf, int len, size_t offset) {
    int current_core = spdk_env_get_current_core();
    service_awaiter awaiter{};
    int rc = spdk_bdev_write(desc, rds[current_core].ch, buf, offset, len,
                             spdk_io_complete_cb, &awaiter.res);
    if (rc == -ENOMEM) {
      // retry queue io
      rds[current_core].bdev_io_wait.bdev = spdk_bdev_desc_get_bdev(desc);
      rds[current_core].bdev_io_wait.cb_fn = spdk_retry_read;
      rds[current_core].context.buf = buf;
      rds[current_core].context.len = len;
      rds[current_core].context.offset = offset;
      rds[current_core].context.res = &awaiter.res;
      rds[current_core].bdev_io_wait.cb_arg = &rds[current_core].context;
      spdk_bdev_queue_io_wait(bdev, rds[current_core].ch,
                              &rds[current_core].bdev_io_wait);
    } else if (rc) {
    }

    return awaiter;
  }

  // 本来不应该有这种用法的，不过既然有直接在当前的reactor上运行是不是也可以，
  // 这种不能产生运行结果，所以不进入tasks中
  void add_task(task<void> &&t) { t.start(); }

  void run(task<int> &&t) {
    tasks.emplace_back(std::move(t));

    // block until all done
    spdk_app_start(&opts, service_init, this);
  }

  template <typename... Args> void run(task<int> &&t, Args... args) {
    tasks.emplace_back(std::move(t));
    run(args...);
  }
};
#endif
