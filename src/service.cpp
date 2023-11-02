#include "service.hpp"
#include "module.hpp"
#include "spdk/bdev.h"
#include "spdk/thread.h"
#include <assert.h>
#include <barrier>
#include <cstdint>
static char json_file[] = "bdev.json";

void spdk_io_complete_cb(struct spdk_bdev_io *bdev_io, bool success,
                         void *cb_arg) {
  spdk_bdev_free_io(bdev_io);
  // resume coroutine
  spdk_service::result *res = (spdk_service::result *)cb_arg;
  res->res = success ? 0 : 1;
  res->coro.resume();
}

void spdk_retry_read(void *args) {
  struct spdk_service::retry_context *ctx =
      (struct spdk_service::retry_context *)args;
  int rc = spdk_bdev_read(ctx->desc, ctx->ch, ctx->buf, ctx->offset, ctx->len,
                          spdk_io_complete_cb, ctx->res);
  if (rc == -ENOMEM) {
    // retry again
    /* spdk_bdev_queue_io_wait(ctx->bdev, ctx->ch, */
    /*                         &rds[current_core].bdev_io_wait); */
  } else if (rc) {
    ctx->res->res = rc;
    ctx->res->coro.resume();
  }
}

void spdk_retry_write(void *args) {
  struct spdk_service::retry_context *ctx =
      (struct spdk_service::retry_context *)args;
  int rc = spdk_bdev_write(ctx->desc, ctx->ch, ctx->buf, ctx->offset, ctx->len,
                           spdk_io_complete_cb, ctx->res);
  if (rc == -ENOMEM) {
    // retry again
    /* spdk_bdev_queue_io_wait(ctx->bdev, ctx->ch, */
    /*                         &rds[current_core].bdev_io_wait); */
  } else if (rc) {
    ctx->res->res = rc;
    ctx->res->coro.resume();
  }
}

void service_threads_exit(void *args) {
  spdk_service *service = (spdk_service *)args;
  spdk_thread_exit(spdk_get_thread());
  service->exit_barrier.arrive_and_drop();
}

void service_exit(void *args) {
  spdk_service *service = (spdk_service *)args;
  uint32_t i;
  SPDK_ENV_FOREACH_CORE(i) {
    if (i == spdk_env_get_current_core())
      continue;
    spdk_thread_send_msg(service->rds[i].thread, service_threads_exit, service);
  }
  service->exit_barrier.arrive_and_wait();
  SPDK_NOTICELOG("Stopping app\n");
  spdk_app_stop(0);
}

void service_thread_run(void *args) {
  task<int> *t = (task<int> *)args;
  t->start();
}

void myapp_bdev_event_cb(enum spdk_bdev_event_type type, struct spdk_bdev *bdev,
                         void *event_ctx) {}
void service_init(void *args) {
  spdk_service *service = (spdk_service *)args;
  // open device
  assert(spdk_bdev_open_ext(service->bdev_name, true, myapp_bdev_event_cb,
                            nullptr, &service->desc) == 0);
  service->bdev = spdk_bdev_desc_get_bdev(service->desc);

  // set cpu
  spdk_cpuset tmpmask;
  for (int i = 0; i < service->num_threads; ++i) {
    SPDK_NOTICELOG("creating schedule thread at core %d\n", i);
    if (i == spdk_env_get_current_core()) {
      service->rds[i].thread = spdk_get_thread();
      service->rds[i].ch = spdk_bdev_get_io_channel(service->desc);
      service->rds[i].context.ch = service->rds[i].ch;
      service->rds[i].context.desc = service->desc;
      continue;
    }
    spdk_cpuset_zero(&tmpmask);
    spdk_cpuset_set_cpu(&tmpmask, i, true);
    // create schedule thread
    service->rds[i].thread = spdk_thread_create(NULL, &tmpmask);
    service->rds[i].ch = spdk_bdev_get_io_channel(service->desc);
    service->rds[i].context.ch = service->rds[i].ch;
    service->rds[i].context.desc = service->desc;
  }

  // round roubin
  for (int i = 0; i < service->tasks.size(); ++i) {
    int core = (i + 1) % service->num_threads;
    spdk_thread_send_msg(service->rds[i].thread, service_thread_run,
                         &service->tasks[i]);
  }

  // waiting for exit
  spdk_thread_send_msg(service->rds[0].thread, service_exit, service);
}
