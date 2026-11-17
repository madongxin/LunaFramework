# P1c（emit 同步 last_present）905 验收（2026-11-15）

客户端：`docs/export-20261115-100522/`  
服务端 A/B：`/root/metrics_P1c_905_{A,B}/`（窗口 149s；导出结束约 10:05:22，A 紧贴结束）

## 结论

**未过。** C1c 略降服务端 `drifted/s`，但客户端 timeout **差于 P1b，更差于 P0**。`keyframe` 仍为 0。

| 指标 | 基线 | P0 | P1b | **P1c** |
|------|------|-----|------|---------|
| timeout | 16.8% | **2.65%** | 3.47% | **5.71%** |
| ack | 83% | 97.4% | 96.5% | 94.1% |
| resend/sent | ~15% | ~2.2% | 2.93% | **5.05%** |
| rpc p99 ms | 1884 | 1839 | 1896 | 1873 |
| AOI 帧/s | ~2514 | ~717 | ~673 | **~608** |
| AOI share | ~74% | ~56% | ~57% | **~57%** |
| `aoi_start_ms` ratio | 0 | 0 | 1 | **1** |
| drifted vs vel | — | — | drifted≫ | **仍 drifted≫**（1473 vs 801 /s） |

## 根因（C1d）

C1c 只修了「emit 之后」的呈现步进。Move 插缝调用 `AllowMoveAoi` 时：

- pred 用墙钟 `now - rec.ms` 往前推  
- `cur` 仍是**尚未 Advance 的** `PresentedX/Z`  
- 匀速 10m/s、间隔 100ms → 误差约 1m → 阈值触发 **假 drifted**，每个 Move 都可能补发  

## 下一刀（C1d）

误差比较改为「呈现点按 vx/vz 积分到 now（钳权威）」再和 pred 比；emit 时把 aoi 追到该 live 点。
