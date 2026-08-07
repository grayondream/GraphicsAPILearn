#!/bin/bash

# 获取 Git 根目录
GIT_ROOT=$(git rev-parse --show-toplevel)

echo "Git root directory: $GIT_ROOT"

# 运行 CMake
cmake -S "$GIT_ROOT" -B "$GIT_ROOT/linux_build" -DCMAKE_TOOLCHAIN_FILE="$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake" 