#!/usr/bin/env bash
# Windows 主机侧全量回归驱动（双副本工作流：WSL 经 interop 驱动 E:\ 工作树的 exe）。
# 用法：
#   ./scripts/run_win.sh all -b dx12 [-d 秒]   # 全量样例
#   ./scripts/run_win.sh <AppName> -b vulkan   # 单样例
# 前置：Windows 树已 pull 且构建（build/src/Release/renderLearn.exe 存在）
set -u
GIT_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
WIN_TREE="${WIN_TREE:-/mnt/e/workspace/GraphicsAPILearn}"
WIN_BUILD_DIR="${WIN_BUILD_DIR:-build}"
EXE="$WIN_TREE/$WIN_BUILD_DIR/src/Release/renderLearn.exe"
DUR=5

args=("$@")
BACKEND="gl"
if [[ "${1:-}" == "all" ]]; then
    MODE=all; shift
else
    MODE=one
fi
while [[ $# -gt 0 ]]; do
    case "$1" in
        -b) BACKEND="$2"; shift 2 ;;
        -d) DUR="$2"; shift 2 ;;
        *) APP_NAME="$1"; shift ;;
    esac
done
[[ -x "$EXE" ]] || { echo "renderLearn.exe 不存在，请先在 Windows 树构建"; exit 2; }

run_one() {
    local app="$1"
    timeout "$DUR" "$EXE" -b "$BACKEND" -a "$app" < /dev/null > "/tmp/opencode/rw_${BACKEND}_${app}.log" 2>&1
}

if [[ "$MODE" == "one" ]]; then
    run_one "${APP_NAME:?用法: run_win.sh <App> -b <backend>}"
    rc=$?
    [[ $rc -eq 124 ]] && echo "OK  ${APP_NAME}($rc)" || echo "FAIL($rc) ${APP_NAME}"
    exit $(( rc == 124 ? 0 : 1 ))
fi

# 全量模式：从 exe -h 输出解析 app 清单（与 scripts/run.sh 同口径）
mapfile -t APPS < <(timeout 5 "$EXE" -h 2>/dev/null | tr -d '\r' | sed -n '/Available apps:/,$p' | tail -1 | tr ' ' '\n' | grep -v '^$')
TOTAL=${#APPS[@]}
OK=0; FAILED=()
for app in "${APPS[@]}"; do
    run_one "$app"
    rc=$?
    if [[ $rc -eq 124 ]]; then
        OK=$((OK+1)); echo "OK  $app"
    else
        FAILED+=("$app($rc)"); echo "FAIL($rc) $app"
    fi
done
echo "==> 完成: $OK OK / ${#FAILED[@]} FAIL / 共 $TOTAL (${BACKEND})"
[[ ${#FAILED[@]} -eq 0 ]]
