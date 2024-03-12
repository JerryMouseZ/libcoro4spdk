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

int num_readers;
int num_writers;
int iterations = 1;

int val = 0;
int thread_num = 1;

void spinreader(int index, int fd) {
  char buffer[128];
  int ret = read(fd, buffer, 1);
  if (ret == 0)
    return;
}

void spinwriter(int index, int fd) {
  char buffer[128];
  int ret = write(fd, buffer, 1);
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

int main(int argc, char* argv[]) {
  args_parse(argc, argv);
  BS::thread_pool pool(thread_num);
  int fd = open("/dev/kernelrcu", O_RDWR);
  assert(fd > 0);
  char buffer[128];

  for (int i = 0; i < num_readers; ++i) {
    pool.detach_task([i, fd]() { spinreader(i, fd); });
  }

  for (int i = 0; i < num_writers; ++i) {
    pool.detach_task([i, fd]() { spinwriter(i, fd); });
  }
  sleep(1);

  // begin test
  int ret = write(fd, buffer, 0);
  if (ret != 0) {
    fprintf(stderr, "start error\n");
  }

  sleep(1);                   // duration
  ret = read(fd, buffer, 0);  // end test
  pool.wait();

  ret = read(fd, buffer, 3);
  if (ret != 0) {
    fprintf(stderr, "get res error\n");
  } else {
    unsigned long read_cnt = *(unsigned long*)buffer;
    unsigned long write_cnt = *(unsigned long*)(buffer + sizeof(unsigned long));
    printf("read ops: %ld\twrite ops: %ld\ttotal ops: %ld\n", read_cnt,
           write_cnt, read_cnt + write_cnt);
  }

  return 0;
}
