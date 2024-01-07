#include "schedule.hpp"
#include <cstdint>
#include "rcu.hpp"

namespace pmss {

// for scheduler
char device_name[16];
char json_file[16];
std::vector<task<int>> tasks;
std::list<task<void>> wrapper_tasks;
int num_threads;
char cpumask[16] = "0x";
spdk_bdev_desc* desc;
spdk_bdev* bdev;
int alive_tasks;
spdk_thread* main_thread = nullptr;

// for per-thread
spdk_thread* threads[256];
spdk_io_channel* channels[256];
spdk_bdev_io_wait_entry wait_entries[256];

void execute() {
  spdk_app_opts opts;
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

  // block until all done
  spdk_app_start(&opts, scheduler_init, nullptr);
}

void run(task<int>&& t) {
  tasks.emplace_back(std::move(t));
  execute();
}

void add_task(task<int>&& t) {
  tasks.emplace_back(std::move(t));
}

void run() {
  execute();
}

template <typename Range>
void run_all(Range& range) {
  for (auto& element : range) {
    tasks.emplace_back(std::move(element));
  }
  execute();
}

void thread_exit(void* args) {
  long core = (long)args;
  spdk_put_io_channel(channels[core]);
  spdk_thread_exit(threads[core]);
}

void service_exit() {
  for (int i = 0; i < num_threads; ++i) {
    if (i == 0) {
      spdk_put_io_channel(channels[i]);
    } else {
      spdk_thread_send_msg(threads[i], thread_exit, (void*)(long)i);
    }
  }
  spdk_bdev_close(desc);
  DEBUG_PRINTF("Stopping app\n");
  spdk_app_stop(0);
}

void task_done(void* args) {
  if (--alive_tasks == 0) {
    service_exit();
  }
}

task<void> task_run(void* args) {
  task<int>* t = (task<int>*)
      args;  // NOTICE：我觉得这里不能固定模板类型，因为本就无法确定task的类型
  co_await* t;
  rcu::rcu_offline();
  spdk_thread_send_msg(main_thread, task_done, nullptr);
}

void service_thread_run(void* args) {
  task<void> t = task_run(args);
  t.start();
  // 要保证t不能被析构
  if (!t.done())
    wrapper_tasks.push_back(std::move(t));
}

void service_thread_run_yield(void* args) {
  std::coroutine_handle<> h = std::coroutine_handle<>::from_address(args);
  h.resume();
}

void myapp_bdev_event_cb(enum spdk_bdev_event_type type, struct spdk_bdev* bdev,
                         void* event_ctx) {}

void thread_init_get_channel(void* args) {
  long core = (long)args;
  // get_io_channel绑定了当前线程，所以需要发给对应的线程去创建
  channels[core] = spdk_bdev_get_io_channel(desc);
}

void scheduler_init(void* args) {
  DEBUG_PRINTF("openning %s\n", device_name);
  // open device
  assert(spdk_bdev_open_ext(device_name, true, myapp_bdev_event_cb, nullptr,
                            &desc) == 0);
  bdev = spdk_bdev_desc_get_bdev(desc);

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
      spdk_thread_send_msg(thread, thread_init_get_channel, (void*)uint64_t(i));
    } else {
      thread = spdk_get_thread();
      main_thread = thread;
      channels[i] = spdk_bdev_get_io_channel(desc);
    }
    threads[i] = thread;
  }

  // round roubin
  for (size_t i = 0; i < tasks.size(); ++i) {
    int core = i % num_threads;
    alive_tasks++;
    spdk_thread_send_msg(threads[core], service_thread_run, &tasks[i]);
  }

  if (tasks.size() == 0)
    service_exit();
}

void init_service(int thread_num, const char* config_file,
                  const char* bdev_name) {
  rcu::rcu_init();
  num_threads = thread_num;
  alive_tasks = 0;
  strncpy(device_name, bdev_name, 15);
  strncpy(json_file, config_file, 15);
}

void deinit_service() {
  spdk_app_fini();
}

// 本来不应该有这种用法的，不过既然有直接在当前的reactor上运行是不是也可以，
// 这种不能产生运行结果，所以不进入tasks中
void launch_task_on_fire(task<void>&& t) {
  t.start();
}
};  // namespace pmss
