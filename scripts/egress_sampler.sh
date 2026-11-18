#!/usr/bin/env bash
# 每 10 秒追加一行 CSV：网关各帧的帧数和 payload、估算线上字节、nstat、网卡 tx。
# 不改发送。计数器是进程启动后的累计值，速率用相邻行的差。
#
#   ./scripts/egress_sampler.sh [秒数] [csv路径] [网卡]
#   ./scripts/egress_sampler.sh --report csv路径
# 秒数为 0 时一直采样，直到被停止。
set -u
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
if [[ "${1:-}" == "--report" ]]; then
  exec python3 "$ROOT/scripts/egress_report.py" "${2:-}"
fi

SECS="${1:-120}"
CSV="${2:-$ROOT/run/formal/egress_sample.csv}"
IFACE="${3:-${LUNASERVER_EGRESS_IFACE:-}}"
URLS="${LUNASERVER_GW_METRICS:-http://127.0.0.1:8080/metrics http://127.0.0.1:8082/metrics}"
mkdir -p "$(dirname "$CSV")"
export EGRESS_SECS="$SECS" EGRESS_CSV="$CSV" EGRESS_IFACE="$IFACE" EGRESS_URLS="$URLS"
python3 - <<'PY'
import csv, os, re, subprocess, time, urllib.request

secs = int(os.environ.get("EGRESS_SECS", "120"))
path = os.environ["EGRESS_CSV"]
iface = os.environ.get("EGRESS_IFACE", "")
urls = os.environ.get("EGRESS_URLS", "").split()
frames = ["move", "reply", "enter", "leave", "heartbeat", "snapshot", "reliable", "other"]
header = ["ts"]
for name in frames:
    header += ["%s_frames" % name, "%s_payload" % name, "%s_wire" % name]
header += ["tcp_out_segs", "tcp_retrans_segs", "tcp_lost_retrans", "nic", "nic_tx_bytes", "nic_tx_dropped"]

def default_iface():
    try:
        text = subprocess.check_output(["ip", "route", "show", "default"], stderr=subprocess.STDOUT).decode("utf-8", "replace")
    except Exception:
        return ""
    parts = text.split()
    if "dev" in parts:
        return parts[parts.index("dev") + 1]
    return ""

def scrape(url):
    try:
        return urllib.request.urlopen(url, timeout=2).read().decode("utf-8", "replace")
    except Exception:
        return ""

def counter(text, metric, frame):
    pat = re.compile(r'^%s\{frame="%s"\}\s+(\d+)' % (re.escape(metric), re.escape(frame)), re.M)
    m = pat.search(text)
    return int(m.group(1)) if m else None

def nstat():
    try:
        text = subprocess.check_output(
            ["nstat", "-a", "TcpRetransSegs", "TcpOutSegs", "TcpExtTCPLostRetransmit"],
            stderr=subprocess.STDOUT).decode("utf-8", "replace")
    except Exception:
        return None, None, None
    def one(key):
        m = re.search(r"^%s\s+(\d+)" % key, text, re.M)
        return int(m.group(1)) if m else None
    return one("TcpOutSegs"), one("TcpRetransSegs"), one("TcpExtTCPLostRetransmit")

def nic_tx(dev):
    if not dev:
        return None, None
    try:
        text = subprocess.check_output(["ip", "-s", "link", "show", "dev", dev],
                                       stderr=subprocess.STDOUT).decode("utf-8", "replace")
    except Exception:
        return None, None
    lines = text.splitlines()
    for i, line in enumerate(lines):
        if line.strip().startswith("TX:"):
            if i + 1 < len(lines):
                nums = lines[i + 1].split()
                if len(nums) >= 4:
                    return int(nums[0]), int(nums[3])
    return None, None

def cell(v):
    return "" if v is None else str(v)

if not iface:
    iface = default_iface()
new = not os.path.exists(path) or os.path.getsize(path) == 0
end = time.time() + secs if secs > 0 else None
while True:
    texts = [scrape(u) for u in urls]
    row = [time.strftime("%Y-%m-%dT%H:%M:%S")]
    for name in frames:
        frames_n = payload = wire = 0
        seen_f = seen_p = seen_w = False
        for text in texts:
            f = counter(text, "lunaserver_tx_frames_by_frame", name)
            p = counter(text, "lunaserver_tx_bytes_by_frame", name)
            w = counter(text, "lunaserver_egress_wire_bytes_estimate", name)
            if f is not None:
                frames_n += f
                seen_f = True
            if p is not None:
                payload += p
                seen_p = True
            if w is not None:
                wire += w
                seen_w = True
        row += [cell(frames_n if seen_f else None), cell(payload if seen_p else None), cell(wire if seen_w else None)]
    out_segs, retrans, lost = nstat()
    tx_b, tx_d = nic_tx(iface)
    row += [cell(out_segs), cell(retrans), cell(lost), iface or "无数据", cell(tx_b), cell(tx_d)]
    with open(path, "a") as f:
        w = csv.writer(f)
        if new:
            w.writerow(header)
            new = False
        w.writerow(row)
    if end is not None and time.time() >= end:
        break
    time.sleep(10)
PY
