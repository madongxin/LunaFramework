#!/usr/bin/env bash
# S3：公开资料 / 在线态 / 世界聊天 TCP 闭环。
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"
# shellcheck disable=SC1090
source "$ROOT/scripts/e2e_inventory.sh"

CLIENT="${ROOT}/build/test/game_tcp_e2e_client"
[[ -x "$CLIENT" ]] || { echo "ERROR: missing $CLIENT" >&2; exit 1; }

export LUNASERVER_RUN_DIR="${LUNASERVER_RUN_DIR:-$ROOT/run/unity-e2e}"
export LUNASERVER_MAP_SHA256_FILE="${LUNASERVER_MAP_SHA256_FILE:-$ROOT/config/maps/map_1001.json.sha256}"
export LUNASERVER_MAP_DATA_VERSION="${LUNASERVER_MAP_DATA_VERSION:-1}"

e2e_ensure_cluster "$ROOT"
# shellcheck disable=SC1090
source "$LUNASERVER_RUN_DIR/E2E_PORTS.env"
HOST="${E2E_HOST:-127.0.0.1}"
GW0="${E2E_GW0_GAME:?}"
GW1="${E2E_GW1_GAME:?}"
e2e_wait_login "$CLIENT" "$HOST" "$GW0"

set +e
out="$("$CLIENT" s3-social "$HOST" "$GW0" "$HOST" "$GW1")"
rc=$?
set -e
echo "$out"
[[ "$rc" -eq 0 ]] || { echo "ERROR: s3-social rc=$rc" >&2; exit "$rc"; }

require() {
  echo "$out" | grep -q "$1" || {
    echo "ERROR: missing $1" >&2
    exit 1
  }
}

require 'brief_by_id_ok=1'
require 'brief_by_name_ok=1'
require 'online_ok=1'
require 'online_state=online'
require 'chat_send_ok=1'
require 'chat_notify_ok=1'
require 'friend_not_implemented=1'
require 'name_ambiguous_ok=1'
require 'name_rate_limited=1'
require 'chat_rate_limited=1'
require 's3_social_ok=1'

echo "test_s3_social.sh PASS"
