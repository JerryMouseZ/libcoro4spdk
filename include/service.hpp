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
#include <cassert>
#include <coroutine>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <list>
#include <pthread.h>
#include <ranges>
#include <vector>
#include "schedule.hpp"

namespace pmss {
// data structure
struct result {
  std::coroutine_handle<> coro;
  int res;
};

struct retry_context {
  void* buf;
  int len;
  size_t offset;
  result* res;
};

struct service_awaiter {
  result res;
  bool ready;
  bool await_ready() { return ready; }
  auto await_suspend(std::coroutine_handle<> coro) { res.coro = coro; }
  auto await_resume() { return res.res; }
  explicit service_awaiter() : ready(false) {}
  void set_failure(int rc) {
    res.res = rc;
    ready = true;
  }
};

extern retry_context retry_contexts[256];

// define api
void spdk_io_complete_cb(struct spdk_bdev_io* bdev_io, bool success,
                         void* cb_arg);

void spdk_retry_read(void* args);

void spdk_retry_write(void* args);

service_awaiter read(void* buf, int len, size_t offset);

service_awaiter write(void* buf, int len, size_t offset);
};  // namespace pmss

#endif
