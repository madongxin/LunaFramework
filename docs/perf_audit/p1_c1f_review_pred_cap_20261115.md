# P1 再复核：C1e 的 2s pred 封顶有毒（C1f，2026-11-15）

## 结论

**上一轮仍不能算改好。** C1e 去掉 0.25s 后却给 pred/live 加了 **2s** 墙钟封顶，和外推设计直接冲突：

- 近处 keyframe = **4s** 才补发  
- 匀速安静超过 **2s** 后：呈现已积分满墙钟，pred 却停在 2s → 误差 10m/s×(t−2) → **假 drifted**  
- 假 drifted 不断重置 `rec.ms` → **keyframe 永远到不了**（解释历次 905 `keyframe=0`）

算术：t=2.2s 时 `err_cm=200`（已超阈值）。

## C1f

- `AllowMoveAoi`：去掉 pred/live 的 2s 封顶（只保留权威钳制）  
- `AdvancePresented`：去掉单步 2s 封顶  
- 单测：`TestC1fPredCapBlocksKeyframe`、`TestC1fSteady20sWithMovesAndKeyframes`

## 过线仍靠 905

timeout ≤ P0 2.65%；`drifted` ≪ `vel_changed`；**`keyframe` 必须出现**。
