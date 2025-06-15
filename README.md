# libcoro4spdk

[Your Logo Here]

## 🚀 One-liner

`libcoro4spdk` is a C++20 library providing coroutine and Read-Copy-Update (RCU) utilities designed for high-performance SPDK-based applications.

## ✨ Features

- coroutines **Coroutine Support**: Leverages C++20 coroutines for asynchronous programming, tailored for SPDK.
- 🔄 **Read-Copy-Update (RCU)**: Provides RCU mechanisms for efficient and scalable data synchronization in concurrent environments.
- ⚡ **High-Performance Focus**: Designed with performance in mind, including benchmarks to validate and track performance.
- lock **Synchronization Primitives**: Offers a suite of synchronization tools like mutexes, condition variables, and spinlocks.
- 💾 **SPDK Integration**: Built to seamlessly integrate with the Storage Performance Development Kit (SPDK).

## 🛠️ Tech Stack

- **C++20**: Utilizes modern C++ features for robust and efficient code.
- **CMake**: Cross-platform build system generator.
- **SPDK (Storage Performance Development Kit)**: Core dependency for high-performance storage applications.
- **Google Test**: For writing and running unit tests.
- **Google Benchmark**: For performance measurement.

## 📂 Project Structure

```
libcoro4spdk/
├── src/                     # Core source files of the library
├── include/                 # Public header files for using the library
├── test/                    # Unit tests (using Google Test)
├── benchmarks/              # Performance benchmark tests
├── bat/                     # Python scripts for running benchmarks and processing results
├── CMakeLists.txt           # Main CMake build script
└── README.md                # This file!
```

## 🚀 Getting Started

Follow these instructions to get a copy of the project up and running on your local machine for development and testing purposes.

### Prerequisites

Ensure you have the following installed:

- **C++20 Compiler**: GCC (10+), Clang (10+), or MSVC (Visual Studio 2019 v16.10+)
- **CMake**: Version 3.22 or higher
- **SPDK**: Follow the official SPDK installation guide for your system. Ensure that the SPDK libraries and headers are accessible.
- **Google Test**: Usually included as a submodule or can be installed via your system's package manager (e.g., `libgtest-dev` on Debian/Ubuntu).
- **Google Benchmark**: Similar to Google Test, it might be a submodule or available via package managers (e.g., `libbenchmark-dev` on Debian/Ubuntu).
- **Python 3**: For running benchmark scripts in the `bat/` directory.

### Installation & Building

1.  **Clone the repository:**
    ```bash
    git clone https://your-repository-url/libcoro4spdk.git # Replace with actual URL
    cd libcoro4spdk
    ```

2.  **Configure with CMake:**
    It's recommended to perform an out-of-source build:
    ```bash
    mkdir build
    cd build
    cmake .. # Add -DSPDK_DIR=/path/to/spdk if SPDK is not found automatically
    ```
    *   If Google Test or Google Benchmark are included as submodules and not found, you might need to initialize them:
        ```bash
        # From the root of the project
        git submodule update --init --recursive
        # Then re-run cmake .. from the build directory
        ```

3.  **Build the project:**
    ```bash
    make
    ```

### Running Tests

To run the unit tests (assuming they are built and enabled via `ENABLE_TEST=ON` in CMake, which is the default):

```bash
# From the build directory
ctest
# Or run individual test executables if you know their names
# e.g., ./test/libcoro4spdk_tests
```

### Running Benchmarks

To run the benchmarks (assuming they are built and enabled via `ENABLE_BENCHMARK=ON` in CMake, which is the default):

1.  Navigate to the build directory where benchmark executables are located (e.g., `build/benchmarks/`).
2.  Run specific benchmark executables directly. For example:
    ```bash
    # From the build directory
    sudo ./benchmarks/colocking_benchmarks # Or other benchmark executables
    ```
3.  Use the Python scripts in the `bat/` directory to run a series of benchmarks and collect results:
    ```bash
    # From the root of the project
    cd bat
    python3 locking.py # Or other scripts like noreading.py
    # Results are typically saved in res/ or noreading/ subdirectories
    ```
    **Note**: Benchmark scripts might require `sudo` privileges and specific configurations.

## 📖 Usage Example (Conceptual)

Below is a conceptual example of how to use the RCU mechanism provided by `libcoro4spdk`. For detailed usage, please refer to the source code and tests.

```cpp
#include "rcu.hpp"
#include <iostream>
#include <string>
#include <vector> // For a more complex data structure example

// Assume MyData is some data structure you want to protect with RCU
struct MyData {
    std::string message;
    std::vector<int> values;
    pmss::rcu::rcu_head rcu; // Embed rcu_head for pmss::rcu::free_rcu
};

MyData* g_my_data_ptr = nullptr; // Global pointer to RCU-protected data

// Function to be called by RCU to free the old data
void free_my_data(pmss::rcu::rcu_head* head) {
    MyData* data = reinterpret_cast<MyData*>(reinterpret_cast<char*>(head) - offsetof(MyData, rcu));
    std::cout << "RCU: Freeing old data: " << data->message << std::endl;
    delete data;
}

// Reader thread/coroutine accessing the data
void reader_example() {
    pmss::rcu::rcu_read_lock();
    MyData* current_data = pmss::rcu::rcu_dereference(g_my_data_ptr);
    if (current_data) {
        std::cout << "Reader: " << current_data->message << std::endl;
        // Process current_data->values, etc.
    }
    pmss::rcu::rcu_read_unlock();
}

// Writer thread/coroutine updating the data
// This would typically be part of a more complex coroutine in an SPDK application
pmss::task<void> writer_example(const std::string& new_message) {
    MyData* new_data = new MyData;
    new_data->message = new_message;
    // new_data->values = ...; // Populate other fields

    MyData* old_data = g_my_data_ptr;
    pmss::rcu::rcu_assign_pointer(g_my_data_ptr, new_data);

    if (old_data) {
        // Instead of pmss::rcu::synchronize_rcu(), which is a blocking call for writers
        // in some RCU implementations, libcoro4spdk's synchronize_rcu() is a coroutine.
        // It yields until all readers that might have seen old_data have finished.
        co_await pmss::rcu::synchronize_rcu();
        // Now it's safe to reclaim old_data.
        // For automatic reclamation using a callback:
        // pmss::rcu::free_rcu(&old_data->rcu); // This is a hypothetical function from the code structure
        // OR, if using the call_rcu from the provided code for custom reclamation:
        pmss::rcu::call_rcu(&old_data->rcu, free_my_data);
        std::cout << "Writer: Scheduled old data for reclamation." << std::endl;
    } else {
        std::cout << "Writer: Initial data published." << std::endl;
    }
    co_return;
}

// main_coroutine() or similar would orchestrate these in an SPDK app
// void setup_and_run() {
//     pmss::rcu::rcu_init(); // Initialize RCU system

//     // Example: Initial data
//     MyData* initial_data = new MyData{ "Hello from initial data!", {}, {} };
//     pmss::rcu::rcu_assign_pointer(g_my_data_ptr, initial_data);

//     // Simulate reader and writer operations (in a real app, these would be concurrent tasks)
//     reader_example();
//     auto writer_task = writer_example("Hello from updated data!");
//     // In a real SPDK app, you'd schedule and run writer_task
//     // For simplicity, this conceptual example doesn't show task execution.

//     // ... later, when shutting down or needing to free the last version
//     // pmss::rcu::synchronize_rcu(); // ensure all readers are done
//     // if (g_my_data_ptr) delete g_my_data_ptr; // simplified cleanup
// }

```
**Note**: The `pmss::rcu::free_rcu` and the exact way `call_rcu` is used for freeing memory for a structure like `MyData` would depend on the final implementation details in `libcoro4spdk`. The example above makes some educated guesses based on common RCU patterns and the provided `rcu.hpp` and `rcu.cpp` snippets. The `offsetof` macro would be needed if `rcu_head` is embedded within `MyData` and `free_my_data` needs to get the pointer to the containing `MyData` structure.

The coroutine `pmss::task<void>` and `co_await` demonstrate integration with the library's asynchronous capabilities.
