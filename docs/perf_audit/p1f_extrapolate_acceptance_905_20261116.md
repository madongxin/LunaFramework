# P1f（去 pred 封顶）905 验收（2026-11-16）

客户端：`docs/export-20261116-001414/`  
服务端 A/B：`/root/metrics_P1f_905_{A,B}/`（109s）

## 结论

**有进步，仍未过 P0。** timeout / resend 好于 P1～P1c，但 timeout **3.12% > P0 2.65%**；服务端 `drifted` 仍略高于 `vel_changed`，`keyframe` 几乎仍为 0。

| 指标 | 基线 | P0 | P1c | **P1f** |
|------|------|-----|------|---------|
| timeout | 16.8% | **2.65%** | 5.71% | **3.12%** |
| ack | 83% | 97.1% | 94.1% | **96.7%** |
| resend/sent | ~15% | 2.22% | 5.05% | **2.65%** |
| rpc p99 ms | 1884 | 1839 | 1873 | 1916 |
| AOI 帧/s | ~2514 | ~717 | ~608 | **~533** |
| AOI share | ~74% | ~56% | ~57% | **~52%** |
| `aoi_start_ms` | 0 | 0 | 1 | **1** |
| drifted vs vel | — | — | 1473 vs 801 | **854 vs 746**（仍略高） |
| keyframe/s | — | — | 0 | **≈0** |

## 解读

- 出口侧：AOI 帧率与占比继续下降，控制面（ack/resend）接近 P0。  
- 外推仍未「站稳」：`keyframe` 起不来说明匀速安静窗口仍被 `drifted`/`vel_changed` 打断。  
- **暂不把 `AOI_EXTRAPOLATE=1` 当默认**；P0 enforce 继续留着。
