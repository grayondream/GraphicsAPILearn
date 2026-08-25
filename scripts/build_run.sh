#!/bin/bash
set -e

# 用法: ./scripts/build_run.sh [build|run|all]
# 默认执行 all: 配置 -> 编译 -> 运行

GIT_ROOT=$(git rev-parse --show-toplevel)
BUILD_DIR="$GIT_ROOT/build"
VCPKG_TOOLCHAIN="${VCPKG_ROOT:-$HOME/apps/vcpkg}/scripts/buildsystems/vcpkg.cmake"
VCPKG_TRIPLET="x64-linux"

MODE="${1:-all}"

configure() {
    echo "==> cmake configure ..."
    cmake -S "$GIT_ROOT" -B "$BUILD_DIR" \
        -DENABLE_OPENGL=ON -DENABLE_VULKAN=ON \
        -DCMAKE_TOOLCHAIN_FILE="$VCPKG_TOOLCHAIN" \
        -DVCPKG_TARGET_TRIPLET="$VCPKG_TRIPLET"
}

build() {
    echo "==> cmake build ..."
    # 并行度固定 8：更高并发在部分环境会拖垮交互（勿改回 nproc/32）
    cmake --build "$BUILD_DIR" --parallel 8
    echo "==> build OK"
}

run() {
    BIN="$BUILD_DIR/src/renderLearn"
    if [ ! -x "$BIN" ]; then
        echo "ERROR: binary not found: $BIN"; exit 1
    fi
    echo "==> run $BIN"
    "$BIN"
    code=$?
    echo "==> run exit code: $code"
    exit "$code"
}

case "$MODE" in
    build) configure && build ;;
    run)   run ;;
    all)   configure && build && run ;;
    *)     echo "usage: $0 [build|run|all]"; exit 1 ;;
esac
