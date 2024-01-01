#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <bits/getopt_core.h>
#include <bits/types/struct_timeval.h>
#include <sys/time.h>
#include <unistd.h>
#include <ctime>
#include <iostream>
#include <vector>
#include "task.hpp"
#include "service.hpp"
#include "spinlock.hpp"
#include "rcu.hpp"
#include "mutex.hpp"

enum LockType { Mutex, SpinLock, RwLock, RCU };
std::vector<std::string> locktypes = {"mutex", "spinlock", "rwlock", "rcu"};
LockType type;
int num_readers;
int num_writers;
int iterations;

int val = 0;
int thread_num = 1;
std::atomic<int> ongoing = 0;
timeval begin;
timeval end;
timeval tbegin, tend;

void print_result(timeval begin, timeval end) {
  printf("type: %s\tnum_readers: %d\tnumwriters: %d\titerations: %d\n",
         locktypes[type].c_str(), num_readers, num_writers, iterations);
  double elapse = (end.tv_sec - begin.tv_sec);
  elapse += double(end.tv_usec - begin.tv_usec) / 1000000;
  printf("%lf s\n", elapse);
}

task<int> reader(int index, int loop, async_simple::coro::Mutex& lock) {
  while (ongoing == 0)
    ;
  gettimeofday(&tbegin, nullptr);
  int res = 0;
  for (int i = 0; i < loop; ++i) {
    co_await lock.coLock();
    res += val;
    lock.unlock();
  }
  gettimeofday(&tend, nullptr);
  co_return res;
}

task<int> reader(int index, int loop, async_simple::coro::SpinLock& lock) {
  while (ongoing == 0)
    ;
  int res = 0;
  for (int i = 0; i < loop; ++i) {
    co_await lock.coLock();
    res += val;
    lock.unlock();
  }
  co_return res;
}

task<int> reader(int index, int loop) {
  while (ongoing == 0)
    ;
  int res = 0;
  for (int i = 0; i < loop; ++i) {
    pmss::rcu::rcu_read_lock();
    res += val;
    pmss::rcu::rcu_read_unlock();
  }
  co_return res;
}

template <typename LockType>
task<int> writer(int index, int loop, LockType& lock) {
  while (ongoing == 0)
    ;
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
  while (ongoing == 0)
    ;
  int res = 0;
  for (int i = 0; i < loop; ++i) {
    co_await rcu_spinlock.coLock();
    int oldval = i;
    oldval += 1;
    val = oldval;
    co_await pmss::rcu::rcu_sync_run();
    rcu_spinlock.unlock();
  }
  co_return 0;
}

void args_parse(int argc, char** argv) {
  if (argc < 2) {
    fprintf(stderr,
            "usage: %s -t [mutex/spinlock/sharedmutex/rcu] -r [reader num] "
            "-w [writer num] "
            "-c [core num]"
            "-i iterations\n",
            argv[0]);
    exit(-1);
  }

  int c;
  while ((c = getopt(argc, argv, "t:r:w:i:c:")) != -1) {
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
        break;
      case 'c':
        thread_num = atoi(optarg);
        break;
    }
  }
}

void run_rcu() {
  for (int i = 0; i < num_writers; ++i)
    pmss::add_task(writer(i, iterations));
  for (int i = 0; i < num_readers; ++i)
    pmss::add_task(reader(i, iterations));
  pmss::run();
}

void benchmark_thread() {
  pmss::init_service(thread_num, "bdev.json", "Malloc0");

  async_simple::coro::Mutex mutex;
  async_simple::coro::SpinLock spinlock;

  if (type == RCU) {
    run_rcu();
    goto done;
  }

  for (int i = 0; i < num_readers; ++i) {
    if (type == Mutex)
      pmss::add_task(reader(i, iterations, mutex));
    else
      pmss::add_task(reader(i, iterations, spinlock));
  }

  for (int i = 0; i < num_writers; ++i) {
    if (type == Mutex)
      pmss::add_task(writer(i, iterations, mutex));
    else
      pmss::add_task(writer(i, iterations, spinlock));
  }

  pmss::run();

done:
  gettimeofday(&end, nullptr);
  print_result(begin, tbegin);
  print_result(tbegin, tend);
  print_result(tend, end);
  pmss::deinit_service();
}

int main(int argc, char* argv[]) {
  args_parse(argc, argv);
  std::thread t(benchmark_thread);
  sleep(1);
  ongoing = 1;
  gettimeofday(&begin, nullptr);
  t.join();
  return 0;
}
