#include "service.hpp"
#include "module.hpp"
#include "spdk/bdev.h"
#include "spdk/thread.h"
#include <assert.h>
#include <barrier>
#include <cstdint>
static char json_file[] = "bdev.json";

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
    spdk_thread_send_msg(service->rds[i].thread, service_threads_exit, NULL);
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

  spdk_cpuset tmpmask;

  for (int i = 0; i < service->num_threads; ++i) {
    SPDK_NOTICELOG("creating schedule thread at core %d\n", i);
    if (i == spdk_env_get_current_core()) {
      service->rds[i].thread = spdk_get_thread();
      service->rds[i].ch = spdk_bdev_get_io_channel(service->desc);
      continue;
    }
    spdk_cpuset_zero(&tmpmask);
    spdk_cpuset_set_cpu(&tmpmask, i, true);
    // create schedule thread
    service->rds[i].thread = spdk_thread_create(NULL, &tmpmask);
    service->rds[i].ch = spdk_bdev_get_io_channel(service->desc);
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
