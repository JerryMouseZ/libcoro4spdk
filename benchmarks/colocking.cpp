#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <bits/getopt_core.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <unistd.h>
#include <iostream>
#include <vector>
#include "schedule.hpp"
#include "task.hpp"
#include "service.hpp"
#include "spinlock.hpp"
#include "rcu.hpp"
#include "mutex.hpp"
#include "sharedmutex.hpp"

enum LockType { Mutex, SpinLock, RwLock, RCU };
std::vector<std::string> locktypes = {"mutex", "spinlock", "rwlock", "rcu"};
LockType type;
int num_readers;
int num_writers;
int thread_num = 1;
std::atomic<int> ongoing = 0;
int cur;
std::atomic<uint64_t> read_cnt = 0;
std::atomic<uint64_t> write_cnt = 0;
int durations = 1;

static inline void begin_test() {
  ongoing.store(1, std::memory_order_release);
}

static inline void end_test() {
  ongoing.store(2, std::memory_order_release);
}

static inline void wait_for_begin() {
  while (ongoing.load() == 0)
    ;
}

struct test_obj {
  int a = 8;
};

volatile test_obj rcu_data[2] = {8, 0x3fffffff};
volatile test_obj* gp = &rcu_data[0];

task<int> reader(int index, async_simple::coro::Mutex& lock) {
  wait_for_begin();
  int res = 0;
  while (1) {
    co_await lock.coLock();
    assert(gp->a == 8);
    lock.unlock();
    ++res;
    if (ongoing.load() != 1) [[unlikely]]
      break;
  }
  read_cnt.fetch_add(res, std::memory_order_relaxed);
  co_return res;
}

task<int> reader(int index, async_simple::coro::SpinLock& lock) {
  wait_for_begin();
  int res = 0;
  while (1) {
    co_await lock.coLock();
    assert(gp->a == 8);
    lock.unlock();
    ++res;
    if (ongoing.load() != 1) [[unlikely]]
      break;
  }
  read_cnt.fetch_add(res, std::memory_order_relaxed);
  co_return res;
}

task<int> reader(int index, async_simple::coro::SharedMutex& lock) {
  wait_for_begin();
  int res = 0;
  while (1) {
    co_await lock.coLockShared();
    assert(gp->a == 8);
    co_await lock.unlockShared();
    ++res;
    if (ongoing.load() != 1) [[unlikely]]
      break;
  }
  read_cnt.fetch_add(res, std::memory_order_relaxed);
  co_return res;
}

task<int> rcureader(int index) {
  wait_for_begin();
  int res = 0;
  while (1) {
    pmss::rcu::rcu_read_lock();
    volatile test_obj* p = pmss::rcu::rcu_dereference(gp);
    assert(p->a == 8);
    pmss::rcu::rcu_read_unlock();
    ++res;
    if (ongoing.load() != 1) [[unlikely]]
      break;
  }
  read_cnt.fetch_add(res, std::memory_order_relaxed);
  co_return res;
}

template <typename LockType>
task<int> writer(int index, LockType& lock) {
  wait_for_begin();
  int res = 0;
  while (1) {
    co_await lock.coLock();
    gp->a = 0;
    gp->a = 8;
    lock.unlock();
    ++res;
    if (ongoing.load() != 1) [[unlikely]]
      break;
  }
  write_cnt.fetch_add(res, std::memory_order_relaxed);
  co_return 0;
}

task<int> writer(int index, async_simple::coro::SharedMutex& lock) {
  wait_for_begin();
  int res = 0;
  while (1) {
    co_await lock.coLock();
    gp->a = 0;
    gp->a = 8;
    co_await lock.unlock();
    ++res;
    if (ongoing.load() != 1) [[unlikely]]
      break;
  }
  write_cnt.fetch_add(res, std::memory_order_relaxed);
  co_return 0;
}

async_simple::coro::Mutex rcu_mutex;
task<int> rcuwriter(int index) {
  wait_for_begin();
  int res = 0;
  while (1) {
    co_await rcu_mutex.coLock();
    cur = !cur;
    volatile test_obj* newp = &rcu_data[cur];
    newp->a = 8;
    volatile test_obj* oldp = pmss::rcu::rcu_dereference(gp);
    pmss::rcu::rcu_assign_pointer(gp, newp);
    co_await pmss::rcu::synchronize_rcu();
    oldp->a = 0;
    rcu_mutex.unlock();
    ++res;
    if (ongoing.load() != 1) [[unlikely]]
      break;
  }
  write_cnt.fetch_add(res, std::memory_order_relaxed);
  co_return 0;
}

void args_parse(int argc, char** argv) {
  if (argc < 2) {
    fprintf(stderr,
            "usage: %s -t [mutex/spinlock/sharedmutex/rcu] -r [reader num] "
            "-w [writer num] "
            "-c [core num]"
            "-d durations(default 1)\n",
            argv[0]);
    exit(-1);
  }

  int c;
  while ((c = getopt(argc, argv, "t:r:w:d:c:")) != -1) {
    switch (c) {
      case 't':
        if (optarg[0] == 'm') {
          type = Mutex;
        } else if (optarg[0] == 's') {
          if (optarg[1] == 'p')
            type = SpinLock;
          else
            type = RwLock;
        } else {
          type = RCU;
        }
        break;
      case 'r':
        num_readers = atoi(optarg);
        break;
      case 'w':
        num_writers = atoi(optarg);
        break;
      case 'd':
        durations = atoi(optarg);
        break;
      case 'c':
        thread_num = atoi(optarg);
        break;
      default:
        fprintf(stderr,
                "usage: %s -t [mutex/spinlock/sharedmutex/rcu] -r [reader num] "
                "-w [writer num] "
                "-c [core num]"
                "-d durations(default 1)\n",
                argv[0]);
        exit(-1);
    }
  }

  printf("type: %s\tnum_readers: %d\tnumwriters: %d\tdurations: %d\n",
         locktypes[type].c_str(), num_readers, num_writers, durations);
}

// print ops
void print_result() {
  double write_ops = write_cnt / (double)durations;
  double read_ops = read_cnt / (double)durations;
  double ops = write_ops + read_ops;
  printf("read_ops: %lf\twrite_ops: %lf\tops: %lf\n", read_ops, write_ops, ops);
}

void benchmark_thread() {
  pmss::init_service(thread_num, "bdev.json", "Malloc0");

  async_simple::coro::Mutex mutex;
  async_simple::coro::SharedMutex smutex;
  async_simple::coro::SpinLock spinlock;

  while (num_readers + num_writers > 0) {
    if (num_readers > 0) {
      if (type == Mutex)
        pmss::add_task(reader(num_readers, mutex));
      else if (type == RwLock) {
        pmss::add_task(reader(num_readers, smutex));
      } else if (type == SpinLock)
        pmss::add_task(reader(num_readers, spinlock));
      else
        pmss::add_task(rcureader(num_readers));
      --num_readers;
    }

    if (num_writers > 0) {
      if (type == Mutex)
        pmss::add_task(writer(num_writers, mutex));
      else if (type == RwLock)
        pmss::add_task(writer(num_writers, smutex));
      else if (type == SpinLock)
        pmss::add_task(writer(num_writers, spinlock));
      else
        pmss::add_task(rcuwriter(num_writers));
      --num_writers;
    }
  }

  pmss::run();
  print_result();
  pmss::deinit_service();
}

int main(int argc, char* argv[]) {
  args_parse(argc, argv);
  std::thread t(benchmark_thread);
  sleep(1);
  begin_test();
  sleep(durations);
  end_test();
  t.join();
  return 0;
}
