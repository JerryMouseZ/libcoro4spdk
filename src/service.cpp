#include "service.hpp"
#include "module.hpp"
#include "schedule.hpp"
#include "spdk/bdev.h"
#include "spdk/env.h"
#include "spdk/log.h"
#include "spdk/thread.h"
#include "task.hpp"
#include <assert.h>
#include <barrier>
#include <coroutine>
#include <cstdint>

namespace pmss {

retry_context retry_contexts[256];
void spdk_io_complete_cb(struct spdk_bdev_io* bdev_io, bool success,
                         void* cb_arg) {
  spdk_bdev_free_io(bdev_io);
  // resume coroutine
  result* res = (result*)cb_arg;
  res->res = success ? 0 : 1;
  res->coro.resume();
}

void spdk_retry_read(void* args) {
  int current_core = spdk_env_get_current_core();
  struct retry_context* ctx = (struct retry_context*)args;
  int rc = spdk_bdev_read(desc, channels[current_core], ctx->buf, ctx->offset,
                          ctx->len, spdk_io_complete_cb, ctx->res);
  if (rc == -ENOMEM) {
    // retry again
    /* spdk_bdev_queue_io_wait(ctx->bdev, ctx->ch, */
    /*                         &rds[current_core].bdev_io_wait); */
  } else if (rc) {
    ctx->res->res = rc;
    ctx->res->coro.resume();
  }
}

void spdk_retry_write(void* args) {
  int current_core = spdk_env_get_current_core();
  struct retry_context* ctx = (struct retry_context*)args;
  int rc = spdk_bdev_write(desc, channels[current_core], ctx->buf, ctx->offset,
                           ctx->len, spdk_io_complete_cb, ctx->res);
  if (rc == -ENOMEM) {
    // retry again
    /* spdk_bdev_queue_io_wait(ctx->bdev, ctx->ch, */
    /*                         &rds[current_core].bdev_io_wait); */
  } else if (rc) {
    ctx->res->res = rc;
    ctx->res->coro.resume();
  }
}

// buf must be dma buffer
service_awaiter read(void* buf, int len, size_t offset) {
  int current_core = spdk_env_get_current_core();
  service_awaiter awaiter{};
  int rc = spdk_bdev_read(desc, channels[current_core], buf, offset, len,
                          spdk_io_complete_cb, &awaiter.res);
  if (rc == -ENOMEM) {
    // retry queue io
    wait_entries[current_core].bdev = bdev;
    wait_entries[current_core].cb_fn = spdk_retry_read;
    wait_entries[current_core].cb_arg = &retry_contexts[current_core];

    // set param to call back
    retry_contexts[current_core].buf = buf;
    retry_contexts[current_core].len = len;
    retry_contexts[current_core].offset = offset;
    retry_contexts[current_core].res = &awaiter.res;
    spdk_bdev_queue_io_wait(bdev, channels[current_core],
                            &wait_entries[current_core]);
  } else if (rc) {
    awaiter.set_failure(rc);
  }

  return awaiter;
}

// read/write根据channel所在的线程，会将io请求发送到对应的spdk线程上
// 可以根据这个进行一些调度
service_awaiter write(void* buf, int len, size_t offset) {
  int current_core = spdk_env_get_current_core();
  service_awaiter awaiter{};
  int rc = spdk_bdev_write(desc, channels[current_core], buf, offset, len,
                           spdk_io_complete_cb, &awaiter.res);
  if (rc == -ENOMEM) {
    // retry queue io
    wait_entries[current_core].bdev = bdev;
    wait_entries[current_core].cb_fn = spdk_retry_write;
    wait_entries[current_core].cb_arg = &retry_contexts[current_core];

    // set param to call back
    retry_contexts[current_core].buf = buf;
    retry_contexts[current_core].len = len;
    retry_contexts[current_core].offset = offset;
    retry_contexts[current_core].res = &awaiter.res;
    spdk_bdev_queue_io_wait(bdev, channels[current_core],
                            &wait_entries[current_core]);
  } else if (rc) {
    awaiter.set_failure(rc);
  }

  return awaiter;
}
};  // namespace pmss
