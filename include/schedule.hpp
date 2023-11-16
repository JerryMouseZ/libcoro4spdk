#ifndef SCHEDULE_HPP_
#define SCHEDULE_HPP_
#pragma once

#include <spdk/bdev.h>
#include "module.hpp"
#include <coroutine>
#include <algorithm>
#include <vector>
#include <list>
#include "task.hpp"

// for scheduler
namespace pmss {
extern char device_name[16];
extern char json_file[16];
extern std::vector<task<int>> tasks;
extern std::list<task<void>> wrapper_tasks;
extern int num_threads;
extern char cpumask[8];
extern spdk_bdev_desc* desc;
extern spdk_bdev* bdev;
extern int alive_tasks;
extern spdk_thread* main_thread;
// for per-thread
extern spdk_thread* threads[256];
extern spdk_io_channel* channels[256];
extern spdk_bdev_io_wait_entry wait_entries[256];

void service_thread_run_yield(void* args);

void scheduler_init(void* args);

struct YieldAwaiter {
  bool await_ready() noexcept { return false; }
  void await_suspend(std::coroutine_handle<> continuation) noexcept {
    spdk_thread_send_msg(spdk_get_thread(), service_thread_run_yield,
                         continuation.address());
  }
  void await_resume() noexcept {}
};

void init_service(int thread_num, const char* config_file,
                  const char* bdev_name);

void deinit_service();

void add_task(task<int>&& t);

void run();

void run(task<int>&& t);

template <typename Range>
void run_all(Range& range);
};  // namespace pmss

static inline struct pmss::YieldAwaiter yield() {
  return pmss::YieldAwaiter{};
}

#endif  // !SCHEDULE_HPP_
