# P1 C1c：emit 与呈现时钟对齐（2026-11-15）

## 为何 C1b 仍不够

C1b 把呈现步进改成墙钟 Δt 后，P1b@905 timeout 3.47%（仍差于 P0 2.65%），`drifted` 仍主导。

剩余缺口：

1. `Move` 插缝 `AllowMoveAoi` 成功时只更新 per-watcher `rec.ms`，**不改** `subject.last_present_ms`
2. 下一拍 `AdvancePresented` 仍按「距上次呈现」迈步（例如距 T=0 已 200ms）
3. pred 只走过「距 emit」的短时间（例如 emit 在 T=180 → 仅 20ms）→ 误差 > 1m → **假 drifted**
4. 假 drifted 反复重置 `rec.ms` → 近处 4s keyframe 仍到不了

算术（10 m/s）：emit@180ms、present@200ms、last_present 仍为 0 → 呈现走 2.0m、pred 走 0.2m → 误差 1.8m。

## C1c 改动

| 点 | 行为 |
|----|------|
| `AllowMoveAoi` 返回 true | `subject.last_present_ms = now`（与 `rec.ms` 对齐） |
| `Move` 改速度 / 首次滑行 | 预置 `last_present_ms = t1` |
| `AdvancePresented` | `last_present_ms<=0` 只打戳不迈步；积分改为 **vx/vz × dt** 再钳权威点 |
| 单测 | `TestC1cEmitSyncsPresentClock` / `TestC1cSteadyWithMidIntervalEmits` |

## 905 验收结果

见 `p1c_extrapolate_acceptance_905_20261115.md`：**未过**（timeout 5.71%）。  
C1c 不够；假 drifted 来自 Move 插缝用冻结 presented 比墙钟 pred → **C1d**（live 积分后再比）。
