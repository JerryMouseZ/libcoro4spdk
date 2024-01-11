#include "include/BS_thread_pool.hpp"
#include <algorithm>
#include <atomic>
#include <bits/getopt_core.h>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <mutex>
#include <pthread.h>
#include <shared_mutex>
#include <sys/time.h>
#include <thread>
#include <unistd.h>
#include <vector>

namespace std {
struct spinlock {
  pthread_spinlock_t _t;
  spinlock() { pthread_spin_init(&_t, PTHREAD_PROCESS_PRIVATE); }
  void lock() { pthread_spin_lock(&_t); }
  void unlock() { pthread_spin_unlock(&_t); }
};
}; // namespace std

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

std::atomic<int> taskcount;

void spinreader(int index, int loop, std::spinlock &lock) {
  while (ongoing == 0)
    ;
  int res = 0;
  for (int i = 0; i < loop; ++i) {
    lock.lock();
    res += val;
    lock.unlock();
  }
  if (taskcount.fetch_add(-1, std::memory_order_relaxed) == 1)
    gettimeofday(&end, nullptr);
}

void mutexreader(int index, int loop, std::mutex &lock) {
  while (ongoing == 0)
    ;
  int res = 0;
  for (int i = 0; i < loop; ++i) {
    lock.lock();
    res += val;
    lock.unlock();
  }
  if (taskcount.fetch_add(-1, std::memory_order_relaxed) == 1)
    gettimeofday(&end, nullptr);
}

void smutexreader(int index, int loop, std::shared_mutex &slock) {
  while (ongoing == 0)
    ;
  int res = 0;
  for (int i = 0; i < loop; ++i) {
    slock.lock_shared();
    res += val;
    slock.unlock_shared();
  }
  if (taskcount.fetch_add(-1, std::memory_order_relaxed) == 1)
    gettimeofday(&end, nullptr);
}

void spinwriter(int index, int loop, std::spinlock &lock) {
  while (ongoing == 0)
    ;
  int res = 0;
  for (int i = 0; i < loop; ++i) {
    lock.lock();
    ++val;
    lock.unlock();
  }
  if (taskcount.fetch_add(-1, std::memory_order_relaxed) == 1)
    gettimeofday(&end, nullptr);
}

void mutexwriter(int index, int loop, std::mutex &lock) {
  while (ongoing == 0)
    ;
  int res = 0;
  for (int i = 0; i < loop; ++i) {
    lock.lock();
    ++val;
    lock.unlock();
  }
  if (taskcount.fetch_add(-1, std::memory_order_relaxed) == 1)
    gettimeofday(&end, nullptr);
}

void smutexwriter(int index, int loop, std::shared_mutex &lock) {
  while (ongoing == 0)
    ;
  int res = 0;
  for (int i = 0; i < loop; ++i) {
    lock.lock();
    ++val;
    lock.unlock();
  }
  if (taskcount.fetch_add(-1, std::memory_order_relaxed) == 1)
    gettimeofday(&end, nullptr);
}

void print_usage(int argc, char **argv) {
  fprintf(stderr,
          "usage: %s -t [mutex/spinlock/sharedmutex/rcu] -r [reader num] "
          "-w [writer num] "
          "-c [core num] "
          "-i iterations\n",
          argv[0]);
}
void args_parse(int argc, char **argv) {
  if (argc < 2) {
    print_usage(argc, argv);
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
    default:
      print_usage(argc, argv);
      exit(-1);
    }
  }
  taskcount = num_readers + num_writers;
}

void print_result(timeval begin, timeval end) {
  printf("type: %s\tnum_readers: %d\tnumwriters: %d\titerations: %d\n",
         locktypes[type].c_str(), num_readers, num_writers, iterations);
  double elapse = (end.tv_sec - begin.tv_sec);
  elapse += double(end.tv_usec - begin.tv_usec) / 1000000;
  printf("%lf s\n", elapse);
}

int main(int argc, char *argv[]) {
  args_parse(argc, argv);

  BS::thread_pool pool(thread_num);
  std::mutex _mutex;
  std::spinlock _spinlock;
  std::shared_mutex _sharedmutex;

  for (int i = 0; i < num_readers; ++i) {
    if (type == Mutex) {
      pool.detach_task([i, &_mutex]() { mutexreader(i, iterations, _mutex); });
    } else if (type == SpinLock) {
      pool.detach_task(
          [i, &_spinlock]() { spinreader(i, iterations, _spinlock); });
    } else {
      pool.detach_task(
          [i, &_sharedmutex]() { smutexreader(i, iterations, _sharedmutex); });
    }
  }

  for (int i = 0; i < num_writers; ++i) {
    if (type == Mutex) {
      pool.detach_task([i, &_mutex]() { mutexwriter(i, iterations, _mutex); });
    } else if (type == SpinLock) {
      pool.detach_task(
          [i, &_spinlock]() { spinwriter(i, iterations, _spinlock); });
    } else {
      pool.detach_task(
          [i, &_sharedmutex]() { smutexwriter(i, iterations, _sharedmutex); });
    }
  }

  sleep(1);
  ongoing = 1;
  gettimeofday(&begin, nullptr);

  pool.wait();

  print_result(begin, end);

  return 0;
}
