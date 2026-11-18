#!/usr/bin/env python3
# 对账服务端已通知可见集合和机器人自己屏幕上的实体。
# 服务端：GET /aoi/visible
#   {"viewers":[{"id":819,"entities":[1,2]}]}
# 机器人导出，两种都收：
#   {"viewers":[{"id":819,"entities":[1,2]}]}
#   {"819":[1,2],"820":[1]}
# 未知 MOVE、LEAVE 后又 MOVE、LEAVE 距离放在 notes 里：
#   {"viewers":[...],"notes":{"unknown_move":3,"move_after_leave":1,"leave_distance":[80]}}
# unknown_move：收到一个自己还没有的实体的 MOVE 的次数。
# leave_distance：收到 LEAVE 时，该实体最后已知距离（米）。
# move_after_leave：收到 LEAVE 之后又收到该实体 MOVE 的次数。
# 缺机器人文件时，幽灵数和缺失数写「无数据」。
import json
import sys


def load(path):
    with open(path, "r") as f:
        return json.load(f)


def index(doc):
    out = {}
    if not isinstance(doc, dict):
        return out
    for row in doc.get("viewers", []):
        out[str(row.get("id"))] = set(int(x) for x in row.get("entities", []))
    # 也可以直接写 {观察者 id: [实体 id, ...]}。viewers / notes 这两个键不当成观察者。
    for key, value in doc.items():
        if key in ("viewers", "notes"):
            continue
        if isinstance(value, list):
            out[str(key)] = set(int(x) for x in value)
    return out


def main(argv):
    if len(argv) == 2 and argv[1] == "--self-test":
        server = {"viewers": [{"id": 1, "entities": [2, 3]}, {"id": 4, "entities": [5]}]}
        client = {"viewers": [{"id": 1, "entities": [2, 9]}, {"id": 4, "entities": [5]}]}
        text = report(server, client)
        if "ghost=1" not in text or "missing=1" not in text:
            sys.stderr.write(text)
            return 1
        print("OK aoi_reconcile self-test")
        return 0
    if len(argv) != 3:
        sys.stderr.write("usage: aoi_reconcile.py server.json client.json\n")
        return 2
    server = load(argv[1])
    try:
        client = load(argv[2])
    except OSError:
        print("client=无数据")
        print("ghost=无数据")
        print("missing=无数据")
        return 0
    print(report(server, client))
    return 0


def report(server, client):
    smap = index(server)
    cmap = index(client)
    rows = []
    ghost = 0
    missing = 0
    viewers = sorted(set(smap) | set(cmap), key=lambda s: int(s) if str(s).isdigit() else s)
    hist = {}
    for vid in viewers:
        s = smap.get(vid, set())
        c = cmap.get(vid, set())
        g = sorted(c - s)
        m = sorted(s - c)
        ghost += len(g)
        missing += len(m)
        hist[len(g)] = hist.get(len(g), 0) + 1
        rows.append((len(g) + len(m), vid, g, m))
    rows.sort(key=lambda r: (-r[0], str(r[1])))
    lines = ["viewers=%d ghost=%d missing=%d" % (len(viewers), ghost, missing)]
    lines.append("ghost_hist=" + ",".join("%s:%s" % (k, hist[k]) for k in sorted(hist)))
    lines.append("top10:")
    shown = 0
    for score, vid, g, m in rows:
        if score == 0 or shown >= 10:
            continue
        lines.append("viewer=%s ghost=%d missing=%d ghost_ids=%s missing_ids=%s" % (vid, len(g), len(m), g, m))
        shown += 1
    if shown == 0:
        lines.append("top10=无")
    extra = client.get("notes", {}) if isinstance(client, dict) else {}
    if "unknown_move" in extra:
        lines.append("unknown_move=%s" % extra["unknown_move"])
    else:
        lines.append("unknown_move=无数据")
    if "move_after_leave" in extra:
        lines.append("move_after_leave=%s" % extra["move_after_leave"])
    else:
        lines.append("move_after_leave=无数据")
    return "\n".join(lines)


if __name__ == "__main__":
    sys.exit(main(sys.argv))
