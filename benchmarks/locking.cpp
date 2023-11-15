#include <cstdio>
#include <cstdlib>
#include <sys/time.h>
#include <iostream>
#include <vector>
#include "task.hpp"
#include "service.hpp"
#include "sharedmutex.hpp"
#include "spinlock.hpp"
#include "rcu.hpp"
#include "mutex.hpp"

int val = 0;

task<int> reader(int index, int loop, async_simple::coro::Mutex& lock) {
  int res = 0;
  for (int i = 0; i < loop; ++i) {
    co_await lock.coLock();
    res += val;
    lock.unlock();
  }
  co_return res;
}

task<int> reader(int index, int loop, async_simple::coro::SpinLock& lock) {
  int res = 0;
  for (int i = 0; i < loop; ++i) {
    co_await lock.coLock();
    res += val;
    lock.unlock();
  }
  co_return res;
}

task<int> reader(int index, int loop, async_simple::coro::SharedMutex& lock) {
  int res = 0;
  for (int i = 0; i < loop; ++i) {
    co_await lock.coLockShared();
    res += val;
    lock.unlockShared();
  }
  co_return res;
}

task<int> reader(int index, int loop) {
  int res = 0;
  for (int i = 0; i < loop; ++i) {
    rcu::rcu_read_lock();
    res += val;
    rcu::rcu_read_unlock();
  }
  co_return res;
}

template <typename LockType>
task<int> writer(int index, int loop, LockType& lock) {
  int res = 0;
  for (int i = 0; i < loop; ++i) {
    co_await lock.coLock();
    ++val;
    lock.unlock();
  }
  co_return 0;
}

async_simple::coro::SpinLock rcu_spinlock;
task<int> writer(int index, int loop) {
  int res = 0;
  for (int i = 0; i < loop; ++i) {
    co_await rcu_spinlock.coLock();
    int oldval = i;
    oldval += 1;
    val = oldval;
    co_await rcu::rcu_sync_run();
    rcu_spinlock.unlock();
  }
  co_return 0;
}

enum LockType { Mutex, SpinLock, RwLock, RCU };
std::vector<std::string> locktypes = {"mutex", "spinlock", "rwlock", "rcu"};
LockType type;
int num_readers;
int num_writers;
int iterations;

void args_parse(int argc, char** argv) {
  if (argc < 2) {
    fprintf(stderr,
            "usage: %s -t [mutex/spinlock/sharedmutex/rcu] -r [reader num] "
            "-w [writer num] "
            "-i iterations\n",
            argv[0]);
    exit(-1);
  }

  int c;
  while ((c = getopt(argc, argv, "t:r:w:i:")) != -1) {
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
      case 'i':
        iterations = atoi(optarg);
    }
  }
}

void print_result(timeval begin, timeval end) {
  printf("type: %s\tnum_readers: %d\tnumwriters: %d\titerations: %d\n",
         locktypes[type].c_str(), num_readers, num_writers, iterations);
  double elapse = (end.tv_sec - begin.tv_sec);
  elapse += double(end.tv_usec - begin.tv_usec) / 1000000;
  printf("%lf s\n", elapse);
}

void run_rcu() {
  for (int i = 0; i < num_writers; ++i)
    g_service->add_task(writer(i, iterations));
  for (int i = 0; i < num_readers; ++i)
    g_service->add_task(reader(i, iterations));
  g_service->run();
}

int main(int argc, char* argv[]) {
  args_parse(argc, argv);
  init_service(16, "bdev.json", "Malloc0");

  timeval begin;
  async_simple::coro::Mutex mutex;
  async_simple::coro::SharedMutex rwlock;
  async_simple::coro::SpinLock spinlock;

  gettimeofday(&begin, nullptr);
  if (type == RCU) {
    run_rcu();
    goto done;
  }

  for (int i = 0; i < num_readers; ++i) {
    if (type == Mutex)
      g_service->add_task(reader(i, iterations, mutex));
    else if (type == RwLock)
      g_service->add_task(reader(i, iterations, rwlock));
    else
      g_service->add_task(reader(i, iterations, spinlock));
  }

  for (int i = 0; i < num_writers; ++i) {
    if (type == Mutex)
      g_service->add_task(writer(i, iterations, mutex));
    else if (type == RwLock)
      g_service->add_task(writer(i, iterations, rwlock));
    else
      g_service->add_task(writer(i, iterations, spinlock));
  }

  g_service->run();

done:
  timeval end;
  gettimeofday(&end, nullptr);
  print_result(begin, end);
  deinit_service();

  return 0;
}
