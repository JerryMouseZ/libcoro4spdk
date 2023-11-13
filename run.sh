#!/bin/bash

cmake -B build -DCMAKE_BUILD_TYPE=Debug -DCMAKE_EXPORT_COMPILE_COMMANDS=ON -Dspdk_root=/home/hjx/spdk -GNinja && ninja -C build