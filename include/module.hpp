#include <assert.h>
#include <barrier>
#include <cstdint>
#include <spdk/bdev.h>
#include <spdk/bdev_module.h>
#include <spdk/bdev_zone.h>
#include <spdk/env.h>
#include <spdk/event.h>
#include <spdk/init.h>
#include <spdk/log.h>
#include <spdk/thread.h>

#ifndef NDEBUG
#define DEBUG_PRINTF(...)                             \
  do {                                                \
    fprintf(stderr, "[%s:%d]: ", __FILE__, __LINE__); \
    fprintf(stderr, __VA_ARGS__);                     \
  } while (0);
#else
#define DEBUG_PRINTF(...) void(0)
#endif  // !NDEBUG
