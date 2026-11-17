# P1@905 `drifted` 偏高根因（2026-11-15）

## 现象

- P1@200：timeout 0%，AOI 占 move+reply ~27%，外推明显省流量。  
- P1@905：timeout 4.79%（略差于纯 P0 2.65%），AOI 帧/s ~681 ≈ P0 ~717，`drifted` 常 ≥ `vel_changed`，`keyframe` 恒为 0。

## 根因（C1b）

两套时钟不一致：

| 路径 | 用的时间 |
|------|----------|
| `AllowMoveAoi` 外推预测 | **墙钟** `elapsed = now - rec.ms` |
| `AdvancePresented` 旧逻辑 | **每次固定** `kTickSec = 0.2` 位移，不管距上次多久 |

因此：

1. `Move()` 刚按墙钟 emit 一次（更新 `rec`）  
2. 几十毫秒后 `AdvancePresented` 仍按 **0.2s × 速度** 推呈现  
3. 预测只走了几十毫秒，呈现走了 0.2s → 误差 > 1m → **`drifted`**  
4. 每次 drifted 都重置 `rec.ms`，近处 4s **keyframe 永远到不了**（故 keyframe=0）  
5. 结果：外推退化成「约每 present/Move 就补发」，满员下和 P0 限流后的 AOI 量差不多，字段更大 → timeout 略差

200 人时 Move/present 交织少、节奏稳，假 drifted 少，所以看起来「很好」。

算术（10 m/s）：墙钟 50ms vs 固定步进 200ms → 误差 1.5m > 阈值 1m。

## 修复

- **C1b**：`last_present_ms` 墙钟步进（已合）。P1b@905 timeout 3.47%，仍不够。  
- **C1c**（本轮）：emit 成功写 `last_present_ms`；呈现用 vx/vz 积分。见 `p1_c1c_emit_present_sync_20261115.md`。

## 验证

同开 `AOI_EXTRAPOLATE=1` + enforce 压 905 一轮即可：

- `drifted` ≪ `vel_changed`，`keyframe` 出现  
- AOI move/s 低于 P0；timeout ≤ P0 2.65%  
