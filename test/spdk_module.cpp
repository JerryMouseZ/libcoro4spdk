#include <gtest/gtest.h>
#include "schedule.hpp"
#include "common.hpp"
TEST(simple_io, spdk_init) {
  pmss::init_service(1, json_file, bdev_dev);
  pmss::run();
  pmss::deinit_service();
  pmss::init_service(1, json_file, bdev_dev);
  pmss::run();
  pmss::deinit_service();
}
