#!/usr/bin/env bash
# 本地集群：2×gateway + 2×session + 2×gamelogic + 1×globalservice + 2×gamedb
# Gateway session_addrs 含双 Session（list:// + rr）
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"

export LUNASERVER_RUN_DIR="${LUNASERVER_RUN_DIR:-$ROOT/run/cluster}"
export LUNASERVER_ADVERTISE_HOST="${LUNASERVER_ADVERTISE_HOST:-127.0.0.1}"

# 复用 formal 端口布局，第二 session 用 8402
export LUNASERVER_SESSION="${LUNASERVER_SESSION:-8401}"
export LUNASERVER_SESSION2="${LUNASERVER_SESSION2:-8402}"
HTTP_S2="${LUNASERVER_HTTP_S2:-8096}"

if [[ -f "$LUNASERVER_RUN_DIR/pids" ]]; then
  echo "cluster already running; ./scripts/stop_formal.sh with LUNASERVER_RUN_DIR=$LUNASERVER_RUN_DIR"
  exit 1
fi

# formal 启动时已把 SESSION2 写入 gateway/gamelogic session_addrs
./scripts/start_formal.sh

SESSION_BIN=""
if [[ -x "$ROOT/build/test/session" ]]; then
  SESSION_BIN="$ROOT/build/test/session"
elif [[ -x "$ROOT/build/test/server" ]]; then
  SESSION_BIN="$ROOT/build/test/server"
fi
if [[ -z "$SESSION_BIN" ]]; then
  echo "ERROR: session binary missing"
  exit 1
fi

mkdir -p "$LUNASERVER_RUN_DIR/logs"
if [[ "$(basename "$SESSION_BIN")" == "server" ]]; then
  LUNASERVER_INSTANCE_ID=sess-1 nohup "$SESSION_BIN" session "$HTTP_S2" "$LUNASERVER_SESSION2" \
    >"$LUNASERVER_RUN_DIR/logs/session2.log" 2>&1 &
else
  LUNASERVER_INSTANCE_ID=sess-1 nohup "$SESSION_BIN" "$HTTP_S2" "$LUNASERVER_SESSION2" \
    >"$LUNASERVER_RUN_DIR/logs/session2.log" 2>&1 &
fi
echo $! >>"$LUNASERVER_RUN_DIR/pids"
echo "started session2 pid=$! port=$LUNASERVER_SESSION2 instance_id=sess-1"
echo "run_cluster_local.sh PASS (session_addrs=127.0.0.1:${LUNASERVER_SESSION},127.0.0.1:${LUNASERVER_SESSION2})"
