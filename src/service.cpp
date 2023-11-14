#include "service.hpp"
#include "module.hpp"
#include "spdk/bdev.h"
#include "spdk/env.h"
#include "spdk/log.h"
#include "spdk/thread.h"
#include "task.hpp"
#include <assert.h>
#include <barrier>
#include <coroutine>
#include <cstdint>

spdk_service* g_service = nullptr;
spdk_thread* main_thread = nullptr;

void spdk_io_complete_cb(struct spdk_bdev_io* bdev_io, bool success,
                         void* cb_arg) {
  spdk_bdev_free_io(bdev_io);
  // resume coroutine
  spdk_service::result* res = (spdk_service::result*)cb_arg;
  res->res = success ? 0 : 1;
  res->coro.resume();
}

void spdk_retry_read(void* args) {
  struct spdk_service::retry_context* ctx =
      (struct spdk_service::retry_context*)args;
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

void spdk_retry_write(void* args) {
  struct spdk_service::retry_context* ctx =
      (struct spdk_service::retry_context*)args;
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

void thread_exit(void* args) {
  spdk_service::reactor_data* rd = (spdk_service::reactor_data*)args;
  spdk_put_io_channel(rd->ch);
  spdk_thread_exit(spdk_get_thread());
}

void service_exit() {
  spdk_bdev_close(g_service->desc);
  for (int i = 0; i < g_service->num_threads; ++i) {
    if (i == 0) {
      spdk_put_io_channel(g_service->rds[i].ch);
    } else {
      spdk_thread_send_msg(g_service->rds[i].thread, thread_exit,
                           &g_service->rds[i]);
    }
  }
  DEBUG_PRINTF("Stopping app\n");
  spdk_app_stop(0);
}

void task_done(void* args) {
  if (--g_service->alive_tasks == 0) {
    service_exit();
  }
}

/* struct task_run_awaitable { */
/*   using handle = std::coroutine_handle<task<int>::promise_type>;  // NOTICE:
 * 感觉此处不能固定模板类型，应该也用模板类型，比如T */
/*   handle _h; */
/*   bool await_ready() const noexcept { return false; } */
/*   auto await_suspend(std::coroutine_handle<> caller) noexcept { */
/*     _h.promise()._caller = caller; */
/*     return _h; */
/*   } */
/*   void await_resume() {} */
/* }; */

task<void> task_run(void* args) {
  task<int>* t = (task<int>*)
      args;  // NOTICE：我觉得这里不能固定模板类型，因为本就无法确定task的类型
  co_await* t;
  // 不理解为什么到这里的时候执行就到了reactor 0上
  // 创建线程的时候明明是另外一个核心上运行的
  spdk_thread_send_msg(main_thread, task_done, nullptr);
}

void service_thread_run(void* args) {
  task<void> t = task_run(args);
  t.start();
  // 要保证t不能被析构
  if (!t.done())
    g_service->wrapper_tasks.push_back(std::move(t));
}

void service_thread_run_yield(void* args) {
  std::coroutine_handle<> h = std::coroutine_handle<>::from_address(args);
  h.resume();
}

bool YieldAwaiter::await_ready() noexcept {
  return false;
}
void YieldAwaiter::await_suspend(
    std::coroutine_handle<> continuation) noexcept {
  spdk_thread_send_msg(spdk_get_thread(), service_thread_run_yield,
                       continuation.address());
}
void YieldAwaiter::await_resume() noexcept {}

void myapp_bdev_event_cb(enum spdk_bdev_event_type type, struct spdk_bdev* bdev,
                         void* event_ctx) {}

void thread_init_get_channel(void* args) {
  spdk_service::reactor_data* rd = (spdk_service::reactor_data*)args;
  // get_io_channel绑定了当前线程，所以需要发给对应的线程去创建
  rd->ch = spdk_bdev_get_io_channel(g_service->desc);
  rd->context.ch = rd->ch;
}

void service_init(void* args) {
  DEBUG_PRINTF("openning %s\n", g_service->device_name);
  // open device
  assert(spdk_bdev_open_ext(g_service->device_name, true, myapp_bdev_event_cb,
                            nullptr, &g_service->desc) == 0);
  g_service->bdev = spdk_bdev_desc_get_bdev(g_service->desc);

  // 难道spdk_thread_create只会创建在当前reactor上吗，不应该吧
  // set cpu
  spdk_cpuset tmpmask;
  spdk_thread* thread;
  uint32_t i;
  SPDK_ENV_FOREACH_CORE(i) {
    DEBUG_PRINTF("creating schedule thread at core %d\n", i);
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
    g_service->rds[i].context.desc = g_service->desc;
    g_service->rds[i].context.bdev = g_service->bdev;
    spdk_thread_send_msg(thread, thread_init_get_channel, &g_service->rds[i]);
  }

  // round roubin
  for (size_t i = 0; i < g_service->tasks.size(); ++i) {
    int core = i % g_service->num_threads;
    g_service->alive_tasks++;
    spdk_thread_send_msg(g_service->rds[core].thread, service_thread_run,
                         &g_service->tasks[i]);
  }
}

void init_service(int thread_num, const char* json_file,
                  const char* device_name) {
  g_service = new spdk_service(thread_num, json_file, device_name);
}

void deinit_service() {
  spdk_app_fini();
  delete g_service;
  g_service = nullptr;
}
