#!/usr/bin/env python3
# 读 egress_sampler.sh 的 CSV，打出整段均值和最忙 60 秒。
# 没有数的格子写「无数据」，不写 0。
import csv
import sys
from datetime import datetime


FRAMES = ["move", "reply", "enter", "leave", "heartbeat", "snapshot", "reliable", "other"]


def num(row, key):
    raw = row.get(key, "")
    if raw is None or raw == "":
        return None
    try:
        return float(raw)
    except ValueError:
        return None


def parse_ts(text):
    try:
        return datetime.strptime(text, "%Y-%m-%dT%H:%M:%S")
    except ValueError:
        return None


def delta(a, b, key):
    x, y = num(a, key), num(b, key)
    if x is None or y is None:
        return None
    return y - x


def fmt_mbps(byte_delta, seconds):
    if byte_delta is None or seconds is None or seconds <= 0:
        return "无数据"
    return "%.3f" % (byte_delta * 8.0 / seconds / 1e6)


def fmt_ratio(frames, segs):
    if frames is None or segs is None or segs <= 0:
        return "无数据"
    return "%.3f" % (frames / segs)


def window(rows, i, j):
    a, b = rows[i], rows[j]
    ta, tb = parse_ts(a["ts"]), parse_ts(b["ts"])
    if ta is None or tb is None:
        return None
    seconds = (tb - ta).total_seconds()
    if seconds <= 0:
        return None
    payload = 0.0
    wire = 0.0
    frames = 0.0
    have_p = have_w = have_f = False
    for name in FRAMES:
        p = delta(a, b, "%s_payload" % name)
        w = delta(a, b, "%s_wire" % name)
        f = delta(a, b, "%s_frames" % name)
        if p is not None:
            payload += p
            have_p = True
        if w is not None:
            wire += w
            have_w = True
        if f is not None:
            frames += f
            have_f = True
    return {
        "seconds": seconds,
        "payload": payload if have_p else None,
        "wire": wire if have_w else None,
        "frames": frames if have_f else None,
        "segs": delta(a, b, "tcp_out_segs"),
        "nic": delta(a, b, "nic_tx_bytes"),
        "i": i,
        "j": j,
    }


def line(title, w):
    if w is None:
        print("%s\t无数据\t无数据\t无数据\t无数据" % title)
        return
    print("%s\t%s\t%s\t%s\t%s" % (
        title,
        fmt_mbps(w["payload"], w["seconds"]),
        fmt_mbps(w["wire"], w["seconds"]),
        fmt_mbps(w["nic"], w["seconds"]),
        fmt_ratio(w["frames"], w["segs"]),
    ))


def main(argv):
    if len(argv) != 2:
        sys.stderr.write("usage: egress_report.py file.csv\n")
        return 2
    try:
        f = open(argv[1], "r")
    except OSError:
        print("窗口\tpayload Mbps\t估算线上 Mbps\t网卡实测 Mbps\t平均每 TCP 报文帧数")
        print("整窗口\t无数据\t无数据\t无数据\t无数据")
        print("最忙60秒\t无数据\t无数据\t无数据\t无数据")
        print("csv=无数据")
        return 0
    with f:
        rows = list(csv.DictReader(f))
    print("窗口\tpayload Mbps\t估算线上 Mbps\t网卡实测 Mbps\t平均每 TCP 报文帧数")
    if len(rows) < 2:
        line("整窗口", None)
        line("最忙60秒", None)
        print("samples=%d" % len(rows))
        return 0
    whole = window(rows, 0, len(rows) - 1)
    best = None
    for i in range(len(rows)):
        for j in range(i + 1, len(rows)):
            w = window(rows, i, j)
            if w is None or w["seconds"] < 50 or w["seconds"] > 70:
                continue
            score = w["nic"] if w["nic"] is not None else w["wire"]
            if score is None:
                continue
            rate = score / w["seconds"]
            if best is None or rate > best[0]:
                best = (rate, w)
    line("整窗口", whole)
    line("最忙60秒", None if best is None else best[1])
    if whole:
        print("整窗口秒数=%.0f" % whole["seconds"])
    if best:
        print("最忙60秒从第 %d 行到第 %d 行，跨度 %.0f 秒" % (best[1]["i"] + 2, best[1]["j"] + 2, best[1]["seconds"]))
    else:
        print("最忙60秒=无数据")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
