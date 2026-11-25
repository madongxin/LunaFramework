#!/usr/bin/env bash
# 配置并编译（默认 Debug；可传 Release / clean）
# 用法：
#   ./scripts/build.sh Debug
#   ./scripts/build.sh Release
#   ./scripts/build.sh clean
#   ENABLE_BRPC=OFF ./scripts/build.sh Debug   # 低层单测构建
#   ./scripts/build.sh Debug --lowlevel
#   LUNASERVER_JOBS=2 ./scripts/build.sh Debug  # 手动指定并行度
#
# 并行度：默认按 MemAvailable 估算（约 1GiB/job），避免小内存机 -j$(nproc) OOM。
# 覆盖：LUNASERVER_JOBS / MMO_JOBS；单 job 预算 LUNASERVER_JOB_MEM_MB（默认 1024）。
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"

# 按可用内存限制 -j，防止 4C/3.6G 无 swap 机器上并行 g++ 触发 oom-killer
lunaserver_default_jobs() {
  local nproc_n total_kb avail_kb per_mb auto
  nproc_n="$(nproc 2>/dev/null || echo 2)"
  [[ "$nproc_n" -lt 1 ]] && nproc_n=1
  total_kb="$(awk '/MemTotal:/ {print $2}' /proc/meminfo 2>/dev/null || echo 0)"
  avail_kb="$(awk '/MemAvailable:/ {print $2}' /proc/meminfo 2>/dev/null || echo 0)"
  if [[ "$avail_kb" -le 0 && "$total_kb" -gt 0 ]]; then
    # 无 MemAvailable 时给 OS/MySQL 预留约 1.5GiB
    if [[ "$total_kb" -gt 1572864 ]]; then
      avail_kb=$((total_kb - 1572864))
    else
      avail_kb=$((total_kb / 2))
    fi
  fi
  per_mb="${LUNASERVER_JOB_MEM_MB:-1024}"
  [[ "$per_mb" -lt 256 ]] && per_mb=256
  if [[ "$avail_kb" -gt 0 ]]; then
    auto=$((avail_kb / (per_mb * 1024)))
  else
    auto=1
  fi
  [[ "$auto" -lt 1 ]] && auto=1
  [[ "$auto" -gt "$nproc_n" ]] && auto=$nproc_n
  # <4GiB 物理内存：硬顶 2，宁可慢也不要卡死整机
  if [[ "$total_kb" -gt 0 && "$total_kb" -lt 4194304 && "$auto" -gt 2 ]]; then
    auto=2
  fi
  echo "$auto"
}

BUILD_TYPE="${1:-${LUNASERVER_BUILD_TYPE:-${MMO_BUILD_TYPE:-Debug}}}"
# Release 默认独立目录，避免覆盖 Debug 的 build/test 业务二进制
if [[ -n "${LUNASERVER_BUILD_DIR:-${MMO_BUILD_DIR:-}}" ]]; then
  BUILD_DIR="${LUNASERVER_BUILD_DIR:-${MMO_BUILD_DIR}}"
elif [[ "$BUILD_TYPE" == "Release" ]]; then
  BUILD_DIR="$ROOT/build-release"
else
  BUILD_DIR="$ROOT/build"
fi

JOBS_SOURCE="auto"
if [[ -n "${LUNASERVER_JOBS:-${MMO_JOBS:-}}" ]]; then
  JOBS="${LUNASERVER_JOBS:-${MMO_JOBS}}"
  JOBS_SOURCE="env"
else
  JOBS="$(lunaserver_default_jobs)"
fi
TARGET=""
LOWLEVEL=0

shift_args=()
if [[ $# -ge 1 ]]; then
  shift || true
fi
while [[ $# -gt 0 ]]; do
  case "$1" in
    --lowlevel) LOWLEVEL=1 ;;
    *)
      if [[ -z "$TARGET" ]]; then
        TARGET="$1"
      else
        shift_args+=("$1")
      fi
      ;;
  esac
  shift || true
done

if [[ "${BUILD_TYPE}" == "clean" ]]; then
  echo "== clean: rm -rf ${BUILD_DIR} =="
  rm -rf "${BUILD_DIR}"
  exit 0
fi

if [[ "$LOWLEVEL" -eq 1 ]]; then
  : "${ENABLE_BRPC:=OFF}"
  : "${ENABLE_MYSQL:=ON}"
  : "${ENABLE_GAME_PROTOBUF:=ON}"
  : "${ENABLE_REDIS:=ON}"
else
  : "${ENABLE_MYSQL:=ON}"
  : "${ENABLE_GAME_PROTOBUF:=ON}"
  : "${ENABLE_REDIS:=ON}"
  : "${ENABLE_BRPC:=ON}"
fi
: "${ENABLE_ROCKSDB:=OFF}"
if [[ "${LUNASERVER_WERROR:-}" == "1" || "${LUNASERVER_WERROR:-}" == "ON" ]]; then
  LUNASERVER_WERROR=ON
else
  LUNASERVER_WERROR=OFF
fi
: "${ENABLE_ASAN:=OFF}"
: "${ENABLE_UBSAN:=OFF}"
: "${ENABLE_TSAN:=OFF}"

if [[ "${ENABLE_ASAN}" == "ON" && "${ENABLE_TSAN}" == "ON" ]]; then
  echo "ERROR: ENABLE_ASAN and ENABLE_TSAN are mutually exclusive" >&2
  exit 1
fi

if [[ "${ENABLE_BRPC}" == "ON" ]]; then
  DEPS_PREFIX="${LUNASERVER_DEPS_PREFIX:-$HOME/.local/lunaserver-deps}"
  if [[ ! -f /usr/local/include/brpc/server.h && ! -f /usr/include/brpc/server.h && \
        ! -f "$DEPS_PREFIX/include/brpc/server.h" ]]; then
    echo "ERROR: ENABLE_BRPC=ON but brpc headers not found." >&2
    echo "       Install via ./scripts/install_deps.sh --build-brpc, or: ./scripts/build.sh ${BUILD_TYPE} --lowlevel" >&2
    exit 1
  fi
  if [[ -f "$DEPS_PREFIX/include/brpc/server.h" ]]; then
    export CMAKE_PREFIX_PATH="${DEPS_PREFIX}${CMAKE_PREFIX_PATH:+:$CMAKE_PREFIX_PATH}"
    export CPATH="${DEPS_PREFIX}/include${CPATH:+:$CPATH}"
    export LIBRARY_PATH="${DEPS_PREFIX}/lib:${DEPS_PREFIX}/lib64${LIBRARY_PATH:+:$LIBRARY_PATH}"
    export LD_LIBRARY_PATH="${DEPS_PREFIX}/lib:${DEPS_PREFIX}/lib64${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
  fi
fi

CXX_BIN="${CXX:-g++}"
echo "== toolchain =="
echo "  compiler : ${CXX_BIN}"
${CXX_BIN} --version | head -1 || true
echo "  build    : ${BUILD_TYPE}"
echo "  cxx std  : C++17 (CMAKE_CXX_STANDARD=17)"
echo "  dir      : ${BUILD_DIR}"
echo "  brpc     : ${ENABLE_BRPC}"
echo "  asan     : ${ENABLE_ASAN}"
echo "  ubsan    : ${ENABLE_UBSAN}"
echo "  tsan     : ${ENABLE_TSAN}"
echo "  jobs     : ${JOBS} (${JOBS_SOURCE}; budget ${LUNASERVER_JOB_MEM_MB:-1024}MB/job)"
if [[ "$JOBS_SOURCE" == "env" ]]; then
  total_kb="$(awk '/MemTotal:/ {print $2}' /proc/meminfo 2>/dev/null || echo 0)"
  if [[ "$total_kb" -gt 0 && "$total_kb" -lt 4194304 && "$JOBS" -gt 2 ]]; then
    echo "WARN: LUNASERVER_JOBS=${JOBS} on <4GiB RAM may OOM (no/low swap). Prefer unset or JOBS<=2." >&2
  fi
fi

echo "== configure: type=$BUILD_TYPE dir=$BUILD_DIR =="
cmake -S "$ROOT" -B "$BUILD_DIR" \
  -DCMAKE_BUILD_TYPE="$BUILD_TYPE" \
  -DCMAKE_CXX_STANDARD=17 \
  -DCMAKE_CXX_STANDARD_REQUIRED=ON \
  -DCMAKE_CXX_EXTENSIONS=OFF \
  -DENABLE_MYSQL="$ENABLE_MYSQL" \
  -DENABLE_GAME_PROTOBUF="$ENABLE_GAME_PROTOBUF" \
  -DENABLE_REDIS="$ENABLE_REDIS" \
  -DENABLE_BRPC="$ENABLE_BRPC" \
  -DENABLE_ROCKSDB="$ENABLE_ROCKSDB" \
  -DENABLE_ASAN="$ENABLE_ASAN" \
  -DENABLE_UBSAN="$ENABLE_UBSAN" \
  -DENABLE_TSAN="$ENABLE_TSAN" \
  -DLUNASERVER_WERROR="$LUNASERVER_WERROR"

echo "== build: jobs=$JOBS target=${TARGET:-all} =="
if [[ -n "$TARGET" ]]; then
  cmake --build "$BUILD_DIR" --target "$TARGET" -j"$JOBS"
else
  cmake --build "$BUILD_DIR" -j"$JOBS"
fi

echo "== done: binaries under ${BUILD_DIR}/test/ =="
ls -la "${BUILD_DIR}/test/" 2>/dev/null | head -20 || true
