#include "service.hpp"
#include "spdk/env.h"
#include "task.hpp"
#include <catch2/catch_all.hpp>
#include <catch2/catch_test_macros.hpp>
#include <cstdio>
#include <vector>

const char* json_file = "bdev.json";
const char* bdev_dev = "Malloc0";
TEST_CASE("simple_io", "simple_write_read") {
  pmss::init_service(1, json_file, bdev_dev);
  pmss::run();
  pmss::deinit_service();
  pmss::init_service(1, json_file, bdev_dev);
  pmss::run();
  pmss::deinit_service();
}
