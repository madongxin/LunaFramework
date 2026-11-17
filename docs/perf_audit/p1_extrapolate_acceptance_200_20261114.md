# P1 外推联调验收（200 人，2026-11-14）

客户端：`docs/export-20261114-235221/`（`run_id=client-20261114-235221`，bot_count=**200**）。  
服务端：`AOI_EXTRAPOLATE=1` + `EGRESS_ENFORCE=1` + C1；A/B 落盘 `/root/metrics_P1_{A,B}/`。

---

## 结论

**200 人联调通过。** 外推字段打通；自己走路超时归零；AOI 占 move+reply wire 约 **27.5%**（结构健康）。可升 **905** 复压。

---

## 客户端

| 指标 | P1@200 | P0@905（对照） | 基线@905 |
|------|--------|----------------|----------|
| timeout 率 | **0%**（0/107263） | 2.65% | 16.8% |
| resend | **0** | ~2.2% | ~15% |
| RPC p50 / p99 (ms) | **73 / 289** | 1511 / 1839 | 1071 / 1884 |
| `aoi_move_with_start_ms` | **29168** | — | — |
| `aoi_move_start_ms_nonzero_ratio` | **1.0** | 0（未开外推） | 0 |
| `aoi_move_per_sec` | ~351 | — | — |
| success_hz &lt; 0.2 | **0** | 32 | 139 |

→ schema/速度透传/外推时钟已生效；不存在「开了外推但 move_start_ms 全 0」。

---

## 服务端 A→B（~80s，online≈201）

| 项 | 值 |
|----|-----|
| HTB 1:10 | ~2.60 Mbps（仍贴 ceil；200 人总量仍偏高，但结构已变） |
| AOI move /s | ~398 |
| move_reply /s | ~1503 |
| AOI 占 move+reply wire | **~27.5%** |
| admitted / defer | 有增长 / ~50/s |
| emit_reason Δ | `vel_changed` 为主，`drifted` 次之；`keyframe`≈0 |

---

## 下一步

1. 升 **905** 同开关复压（主验收）。  
2. 905 后再看 G1（是否还能低于 ceil）与 `drifted` 是否可再降。  
