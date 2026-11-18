#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
export LUNASERVER_FORMAL="${LUNASERVER_FORMAL:-1}"
export LUNASERVER_ADVERTISE_HOST="${LUNASERVER_ADVERTISE_HOST:-127.0.0.1}"
export LUNASERVER_HTTP_BIND="${LUNASERVER_HTTP_BIND:-127.0.0.1}"
export LUNASERVER_ENABLE_ADMIN="${LUNASERVER_ENABLE_ADMIN:-0}"
exec "$ROOT/scripts/start_formal.sh" "$@"
