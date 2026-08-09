#!/bin/bash
set -e

# 用法:
#   ./scripts/run.sh [options...]             透传参数运行 renderLearn（如 -h / -a Triangle / -b gl）
#   ./scripts/run.sh all [backend] [duration]  运行某后端所有 example，每个运行 duration 秒（默认 5s）
#     例: ./scripts/run.sh all          # gl 后端，每个 5s
#         ./scripts/run.sh all gl 3     # gl 后端，每个 3s

GIT_ROOT=$(git rev-parse --show-toplevel)
BIN="$GIT_ROOT/build/src/renderLearn"

if [ ! -x "$BIN" ]; then
    echo "ERROR: binary not found: $BIN (先执行 ./scripts/build_run.sh build)"
    exit 1
fi

run_all() {
    local backend="${1:-gl}"
    local duration="${2:-5}"
    if ! [ "$duration" -ge 1 ] 2>/dev/null; then
        echo "ERROR: duration 必须为正整数（当前: $duration）"
        exit 1
    fi

    local help_out apps app code total=0 ok=0 fail=0 log
    help_out=$("$BIN" -h 2>&1)
    apps=$(echo "$help_out" | awk '
        /^Available apps:/ {sub(/^Available apps:[ ]*/, ""); if (NF) print; found=1; next}
        found && NF {print}
    ' | tr -s ' ')

    if [ -z "$apps" ]; then
        echo "ERROR: 无法从 '$BIN -h' 输出解析 app 列表"
        exit 1
    fi

    echo "==> backend=$backend, 共 $(echo "$apps" | wc -w) 个 example, 每个运行 ${duration}s"

    for app in $apps; do
        total=$((total + 1))
        log="/tmp/renderLearn_${backend}_${app}.log"
        code=0
        timeout -k 2 "$duration" "$BIN" -b "$backend" -a "$app" >"$log" 2>&1 || code=$?
        if { [ "$code" = "0" ] || [ "$code" = "124" ]; } && ! grep -q "\[error\]" "$log"; then
            echo "[ OK ] ($total) $app"
            ok=$((ok + 1))
        else
            echo "[FAIL] ($total) $app  (exit=$code, 日志: $log)"
            grep -m3 -E "Assertion|\[error\]|Failed" "$log" 2>/dev/null | sed 's/^/       /'
            fail=$((fail + 1))
        fi
    done

    echo "==> 完成: $ok OK / $fail FAIL / 共 $total"
    [ "$fail" -eq 0 ]
}

if [ "${1:-}" = "all" ]; then
    shift
    run_all "$@"
    exit $?
fi

exec "$BIN" "$@"
