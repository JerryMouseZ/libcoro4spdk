cmake_minimum_required(VERSION 3.22)
project(libcoro4spdk)

enable_testing()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_COMPILER clang++)

# Set SPDK paths
set(spdk_root $ENV{spdk_root} CACHE PATH "Path to SPDK")
if(NOT spdk_root)
    set(spdk_root ${spdk_root} CACHE PATH "Path to SPDK" FORCE)
endif()

# Add SPDK include directory
include_directories(${spdk_root}/include)

# Add SPDK library path
link_directories(${spdk_root}/build/lib)

if (CMAKE_BUILD_TYPE MATCHES Debug)
  add_compile_options(-fsanitize=address -fno-omit-frame-pointer -g)
  set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fsanitize=address")
endif ()
add_compile_options(-Werror)

OPTION(ENABLE_TEST "on for build tests and unit tests" ON)
OPTION(ENABLE_BENCHMARK "on for benchmarks" ON)
include_directories(include)

add_subdirectory(src)

# set(spdk_root /home/spdk)
if (ENABLE_BENCHMARK)
  find_package(benchmark REQUIRED)
  add_subdirectory(benchmarks)
endif()

if (ENABLE_TEST)
  find_package(GTest REQUIRED)
  add_subdirectory(test)
endif()
