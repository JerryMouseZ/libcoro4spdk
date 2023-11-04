#include "service.hpp"
#include "module.hpp"
#include "spdk/bdev.h"
#include "spdk/log.h"
#include "spdk/thread.h"
#include <assert.h>
#include <barrier>
#include <cstdint>

spdk_service *g_service = nullptr;
spdk_thread *main_thread = nullptr;

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

void service_exit(void *args) {
  if (--g_service->alive_reactors == 0) {
    spdk_bdev_close(g_service->desc);
    SPDK_NOTICELOG("Stopping app\n");
  }
  spdk_app_stop(0);
}

// 每个线程的最后一个task完成时退出当前spdk线程
// 所有task都退出时提醒main_thread调用app_stop
void task_done() {
  int current_core = spdk_env_get_current_core();
  if (--g_service->rds[current_core].alive_tasks == 0) {
    spdk_put_io_channel(g_service->rds[current_core].ch);
    spdk_thread_exit(spdk_get_thread());
    spdk_thread_send_msg(main_thread, service_exit, nullptr);
  }
}

task<void> task_run(void *args) {
  task<int> *t = (task<int> *)args;
  co_await *t;
  task_done();
}

void service_thread_run(void *args) {
  task<void> t = task_run(args);
  t.start();
}

void myapp_bdev_event_cb(enum spdk_bdev_event_type type, struct spdk_bdev *bdev,
                         void *event_ctx) {}
void service_init(void *args) {
  SPDK_NOTICELOG("openning %s\n", g_service->device_name);
  // open device
  assert(spdk_bdev_open_ext(g_service->device_name, true, myapp_bdev_event_cb,
                            nullptr, &g_service->desc) == 0);
  g_service->bdev = spdk_bdev_desc_get_bdev(g_service->desc);

  // set cpu
  spdk_cpuset tmpmask;
  spdk_thread *thread;
  for (int i = 0; i < g_service->num_threads; ++i) {
    SPDK_NOTICELOG("creating schedule thread at core %d\n", i);
    if (i != spdk_env_get_current_core()) {
      // create schedule thread
      spdk_cpuset_zero(&tmpmask);
      spdk_cpuset_set_cpu(&tmpmask, i, true);
      thread = spdk_thread_create(NULL, &tmpmask);
    } else {
      thread = spdk_get_thread();
      main_thread = thread;
    }
    g_service->rds[i].thread = thread;
    g_service->rds[i].ch = spdk_bdev_get_io_channel(g_service->desc);
    g_service->rds[i].context.ch = g_service->rds[i].ch;
    g_service->rds[i].context.desc = g_service->desc;
    g_service->rds[i].context.bdev = g_service->bdev;
  }

  // round roubin
  for (int i = 0; i < g_service->tasks.size(); ++i) {
    int core = i % g_service->num_threads;
    g_service->rds[core].alive_tasks++;
    spdk_thread_send_msg(g_service->rds[i].thread, service_thread_run,
                         &g_service->tasks[i]);
  }
}

void init_service(int thread_num, const char *json_file,
                  const char *device_name) {
  g_service = new spdk_service(thread_num, json_file, device_name);
}

void deinit_service() {
  delete g_service;
  g_service = nullptr;
}
