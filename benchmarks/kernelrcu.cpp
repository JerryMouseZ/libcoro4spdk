#include <algorithm>
#include "include/BS_thread_pool.hpp"
#include <assert.h>
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <fcntl.h>
#include <functional>
#include <iostream>
#include <mutex>
#include <pthread.h>
#include <shared_mutex>
#include <stdio.h>
#include <stdlib.h>
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
};  // namespace std

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

void spinreader(int index, int loop, int fd) {
  char buffer[128];
  int ret = read(fd, buffer, loop);
  if (ret == 0)
    return;
}

void spinwriter(int index, int loop, int fd) {
  char buffer[128];
  int ret = write(fd, buffer, loop);
  if (ret == 0)
    return;
}

void print_usage(int argc, char** argv) {
  fprintf(stderr,
          "usage: %s -r [reader num] "
          "-w [writer num] "
          "-i iterations\n",
          argv[0]);
}
void args_parse(int argc, char** argv) {
  if (argc < 2) {
    print_usage(argc, argv);
    exit(-1);
  }

  int c;
  while ((c = getopt(argc, argv, "r:w:i:c:")) != -1) {
    switch (c) {
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
}

void print_result(timeval begin, timeval end) {
  printf("type: %s\tnum_readers: %d\tnumwriters: %d\titerations: %d\n",
         locktypes[type].c_str(), num_readers, num_writers, iterations);
  double elapse = (end.tv_sec - begin.tv_sec);
  elapse += double(end.tv_usec - begin.tv_usec) / 1000000;
  printf("%lf s\n", elapse);
}

int main(int argc, char* argv[]) {
  args_parse(argc, argv);
  BS::thread_pool pool(thread_num);
  int fd = open("/dev/kernelrcu", O_RDWR);
  assert(fd > 0);
  char buffer[128];

  for (int i = 0; i < num_readers; ++i) {
    pool.detach_task([i, fd]() { spinreader(i, iterations, fd); });
  }

  for (int i = 0; i < num_writers; ++i) {
    pool.detach_task([i, fd]() { spinwriter(i, iterations, fd); });
  }
  sleep(1);

  int ret = write(fd, buffer, 0);
  if (ret != 0) {
    fprintf(stderr, "start error\n");
  }

  pool.wait();

  ret = read(fd, buffer, 0);
  if (ret != 0) {
    fprintf(stderr, "get res error\n");
  }

  return 0;
}
