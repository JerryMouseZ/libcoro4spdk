/*
 * Copyright (c) 2022, Alibaba Group Holding Limited;
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#ifndef ASYNC_SIMPLE_CORO_SPIN_LOCK_H
#define ASYNC_SIMPLE_CORO_SPIN_LOCK_H

#include "service.hpp"
#include <atomic>
#include <cassert>
#include <coroutine>
#include <mutex>

namespace async_simple {
namespace coro {

class SpinLock {
 public:
  explicit SpinLock(std::int32_t count = 1024) noexcept
      : _spinCount(count), _locked(false) {}

  bool tryLock() noexcept {
    return !_locked.exchange(true, std::memory_order_acquire);
  }

  task<int> coLock() noexcept {
    auto counter = _spinCount;
    while (!tryLock()) {
      while (_locked.load(std::memory_order_relaxed)) {
        if (counter-- <= 0) {
          co_await YieldAwaiter();
          counter = _spinCount;
        }
      }
    }
    co_return 0;
  }

  void unlock() noexcept { _locked.store(false, std::memory_order_release); }

 private:
  std::int32_t _spinCount;
  std::atomic<bool> _locked;
};

}  // namespace coro
}  // namespace async_simple

#endif  // ASYNC_SIMPLE_CORO_SPIN_LOCK_H
