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