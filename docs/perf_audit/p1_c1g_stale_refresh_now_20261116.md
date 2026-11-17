# P1 C1g：RefreshViewers 旧 now → 假 drifted（2026-11-16）

## 现象（P1f@905）

- timeout 3.12%（仍差于 P0 2.65%）  
- `drifted` 854/s 略高于 `vel_changed` 746/s  
- `keyframe` ≈ 0（109s 仅 1 次）

## 根因

`AdvancePresented` 换格后调用 `RefreshViewers(..., mover_id)`，其中：

```cpp
AllowMoveAoi(..., mit->second.last_move_server_ms)
```

`last_move_server_ms` 常早于最近一次呈现。此时：

1. `aoi` 已按墙钟推进  
2. `now` 仍是旧 Move 时间 → live 不积分，pred 几乎停在 emit 点  
3. 误差 > 1m → **假 drifted**  
4. `last_present_ms = 旧 now` → **呈现时钟回拨**，后续步进更乱，keyframe 年龄反复被打断  

算术：emit@1000、present@1200 后 aoi=+2m，再用 now=1000 比较 → err=200cm。

## 修复

1. `RefreshViewers` 改为 `AllowMoveAoi(..., NowMs())`  
2. `AllowMoveAoi`：`now = max(now_ms, last_present_ms)`，禁止回拨  
3. 单测：`TestC1gStaleNowVsAdvancedPresent`
