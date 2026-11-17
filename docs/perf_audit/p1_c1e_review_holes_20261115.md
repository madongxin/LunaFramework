# P1 复核：C1d 未闭环 + C1e（2026-11-15）

## 复核结论

**不能**说「C1d 已改好、可以直接过 905」。

| 项 | 状态 |
|----|------|
| C1d：Move 插缝冻结 presented vs 墙钟 pred | 算术正确，代码已合 |
| 单测 | 只覆盖公式草图，**未**跑进 `MapRuntime::AllowMoveAoi` |
| P1c 实测 | timeout 5.71%，证明仅 C1c/当时线上逻辑不够 |
| **C1e-1** | present `Δt` 硬顶 0.25s，片延迟时 pred 仍走满墙钟 → 假 drifted（1s 延迟约 7.5m） |
| **C1e-2** | live 钳权威、pred 不钳 → 路段末端假 drifted |

## C1e 已补

1. present / live / pred 墙钟上限改为 **2.0s**（只防极端卡顿）  
2. pred 与 live **同样钳到权威点** 再算误差  

## 仍需 905 实测才能宣称过线

静态复核无法替代满员出口；过线标准不变：timeout ≤ P0 2.65%，`drifted` ≪ `vel_changed`，`keyframe` 出现。
