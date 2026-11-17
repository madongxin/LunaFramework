# P1 C1d：Move 插缝用 live 呈现比 pred（2026-11-15）

## 背景

P1c@905 timeout **5.71%**（差于 P0 2.65% / P1b 3.47%），`drifted` 仍 ≫ `vel_changed`。

## 根因

`AllowMoveAoi` 在 Move 路径上用冻结的 `PresentedX/Z` 与墙钟 pred 比较。呈现线程尚未 Advance 时，匀速也会每 100ms 左右超过 100cm → 假 `drifted` 刷屏。

## 修复

1. `cur = presented + vel * min(0.25, now - last_present_ms)`，并钳到权威点  
2. 用该 live 点算误差；批准 emit 时把 `aoi_x/z` 追到 live，再写 `rec` / `last_present_ms`  
3. 单测：`TestC1dLivePresentVsWallPred`

## 验收（只压 905 一轮）

`AOI_EXTRAPOLATE=1` + `EGRESS_ENFORCE=1`；客户端外推保持开。

| 指标 | 必须 |
|------|------|
| timeout | **≤ P0 2.65%** |
| `emit_reason` | `drifted` ≪ `vel_changed`；`keyframe` 出现 |
| AOI 帧/s | 明显低于 P0 ~717 |
