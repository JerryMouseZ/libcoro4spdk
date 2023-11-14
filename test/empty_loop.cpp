#include "task.hpp"
#include <assert.h>

task<int> empty() {
  co_return 2;
}
task<int> empty_loop() {
  int r = 0;
  for (int i = 0; i < 100000; ++i) {
    r += co_await empty();
  }
  co_return r;
}

int main(int argc, char* argv[]) {
  auto t = empty_loop();
  t.start();

  assert(t.get().value() == 100000 * 2);

  return 0;
}
