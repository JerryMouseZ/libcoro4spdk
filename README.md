# `libcoro4spdk`

<p align="center">
  <!-- 您可以在这里放置一个项目 Logo -->
  <img src="https://placehold.co/150x150/8a2be2/ffffff?text=Logo" alt="项目Logo占位符">
</p>

> **一句话简介:** 一个为 SPDK 设计的、基于 C++20 协程的高性能异步编程库，旨在简化 SPDK 的开发模式，同时保持其高性能特性。

---

### ✨ 功能特性 (Features)

* **⚡️ 协程化 SPDK I/O**: 将 SPDK 的原生异步回调模型封装成现代 C++20 协程。开发者可以使用 `co_await` 关键字来编写逻辑清晰、易于维护的同步风格代码，而底层仍然享受 SPDK 的高性能 I/O。

* **🛡️ 协程同步原语**: 提供了一整套协程安全的同步原语，包括 `Mutex`、`SpinLock`、`SharedMutex` 和 `ConditionVariable`。这些工具使得在协程环境中管理并发和共享资源变得简单而安全。

* **🔄 高效的 RCU 实现**: 内置了用户态的读-复制-更新 (Read-Copy-Update, RCU) 机制，并提供了 `synchronize_rcu` 等协程接口。这为高并发、读多写少的场景提供了极低开销的同步方案。

* **🚀 SPDK 线程调度器**: 实现了一个与 SPDK 紧密集成的协程调度器，可以将不同的协程任务分发到 SPDK 的 `reactor` 线程上执行，充分利用多核优势并遵循 SPDK 的无锁线程模型。

* **📊 内置性能测试套件**: 项目包含一个全面的基准测试模块 (`benchmarks`)，用于评估和比较不同锁机制（如协程锁、POSIX 锁、RCU）的性能，并提供了 Python 脚本自动化执行和数据处理。

---

### 💻 技术栈 (Tech Stack)

* **语言**: C++20
* **核心框架**: SPDK (Storage Performance Development Kit)
* **构建系统**: CMake & Ninja
* **编译器**: Clang++
* **测试框架**: Google Test
* **性能测试**: Google Benchmark
* **脚本**: Python

---

### 📂 项目结构 (Project Structure)

```
.
├── benchmarks/      # 性能基准测试代码
│   ├── colocking.cpp  # 协程锁性能测试
│   └── kernelrcu.c    # 内核RCU模块对比测试
├── bat/             # 运行基准测试和处理结果的Python脚本
├── cds/             # 并发数据结构 (如RCU安全链表)
├── include/         # 库的公开头文件
│   ├── schedule.hpp # 协程调度器
│   ├── service.hpp  # SPDK I/O服务封装
│   ├── task.hpp     # 核心协程任务(task)实现
│   ├── rcu.hpp      # RCU实现
│   └── mutex.hpp    # 协程互斥锁
├── src/             # 库的源文件实现
│   ├── schedule.cpp # 调度器实现
│   ├── service.cpp  # I/O服务实现
│   └── rcu.cpp      # RCU实现
├── test/            # 使用 Google Test 编写的单元/集成测试
│   ├── spdk_io.cpp  # SPDK I/O 功能测试
│   └── rcu.cpp      # RCU 功能测试
├── CMakeLists.txt   # 主构建脚本
├── run.sh           # 快速构建脚本
└── bdev.json        # SPDK bdev 配置文件示例
```

---

### 🚀 安装与启动 (Getting Started)

请按照以下步骤来设置和启动项目。

#### 1. 前置依赖

在开始之前，请确保您已经安装并配置好以下环境：

* **SPDK**: 这是核心依赖。请按照 [SPDK 官方文档](https://spdk.io/doc/getting_started.html) 进行安装，并设置好 Hugepages。
* **Google Test**: 用于运行测试。
* **Google Benchmark**: 用于运行性能测试。
* **Clang/LLVM**: 项目推荐使用 Clang++ 作为编译器。
* **Ninja**: 一个快速的构建工具。

#### 2. 克隆仓库

```bash
git clone <your-repository-url>
cd libcoro4spdk
```

#### 3. 配置与构建

项目提供了一个 `run.sh` 脚本来简化构建过程。您需要根据您本地的 SPDK 安装路径修改脚本中的 `-Dspdk_root` 参数。

例如，如果您的 SPDK 安装在 `/path/to/your/spdk`：

**修改 `run.sh`**:

```bash
#!/bin/bash

# 将 /home/hjx/spdk 替换为你的 SPDK 实际路径
cmake -B build -DCMAKE_BUILD_TYPE=Debug -DCMAKE_EXPORT_COMPILE_COMMANDS=ON -Dspdk_root=/path/to/your/spdk -GNinja && ninja -C build
```

**执行构建**:

```bash
chmod +x run.sh
./run.sh
```

这将在 `build` 目录下生成所有可执行文件，包括库、测试和性能基准测试程序。

---

### 💡 使用示例 (Usage)

以下是一个简单的示例，展示了如何使用该库来执行一次异步的“写后读”操作。这个例子来自于 `test/spdk_io.cpp`。

```cpp
#include "service.hpp" // 引入 SPDK I/O 服务
#include "task.hpp"      // 引入协程 task
#include "schedule.hpp"  // 引入调度器
#include "common.hpp"    // 引入配置文件路径
#include <gtest/gtest.h>
#include "spdk/env.h"

// 定义一个返回 task<int> 的协程函数
task<int> simple_write_read() {
  // 分配一个用于 DMA 的内存缓冲区
  char* dma_buf = (char*)spdk_dma_zmalloc(4096, 4096, nullptr);
  
  // 准备数据并异步写入
  strcpy(dma_buf, "hello world");
  int rc = co_await pmss::write(dma_buf, 4096, 0);
  EXPECT_TRUE(rc == 0); // 检查写入是否成功

  // 清空缓冲区并异步读回
  memset(dma_buf, 0, 4096);
  rc = co_await pmss::read(dma_buf, 4096, 0);
  EXPECT_TRUE(rc == 0); // 检查读取是否成功

  // 验证数据
  EXPECT_TRUE(strcmp(dma_buf, "hello world") == 0);
  fprintf(stderr, "simple write read done\n");

  spdk_dma_free(dma_buf);
  co_return 0;
}

// 主函数或测试用例
TEST(simple_io, simple_write_read) {
  // 1. 初始化服务，指定线程数和配置文件
  pmss::init_service(1, json_file, bdev_dev);
  
  // 2. 将协程任务交给调度器并运行
  pmss::run(simple_write_read());
  
  // 3. 运行结束后，反初始化服务
  pmss::deinit_service();
}
```

这个例子清晰地展示了如何使用 `co_await` 来代替传统的回调函数，使得异步代码的编写变得更加直观和简单。
