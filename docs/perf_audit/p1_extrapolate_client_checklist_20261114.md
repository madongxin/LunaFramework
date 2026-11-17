# P1 外推：客户端配合清单（2026-11-14）

服务端外推 + egress enforce：P1g@905 验收后 **默认开**（`GAMEMESH_AOI_EXTRAPOLATE` / `GAMEMESH_EGRESS_ENFORCE`；显式 `=0` 关闭）。  
灰度打开前，客户端需具备下列能力；否则不要开开关。

---

## 1. 协议 / Hello

| 项 | 要求 |
|----|------|
| Schema | 与带 `vx/vz/move_start_ms` 的 `EntitySnapshot` 一致（字段 8/9/10） |
| Hello | 使用新 `schema_sha256`；旧客户端会被 schema mismatch 拒连 |
| 兼容策略 | **不**做双摘要；开外推 = 客户端必须同时升级 |

---

## 2. 收包语义

| 事件 | 客户端行为 |
|------|------------|
| ENTER (`op=1`) | 读 `position` + `vx/vz/move_start_ms`，作为外推起点 |
| MOVE (`op=2`) | 用权威/呈现 `position` 覆盖本地，并刷新速度段 |
| LEAVE | 不依赖速度；删实体 |
| 速度为 0 | 停在 `position`，不再外推 |

---

## 3. 外推算法（与服务端一致）

```
估计服务器现在 = 本地现在 + (最近一帧 server_time_ms − 本地接收时刻)
位置 ≈ 上次收到的 position + (vx, vz) * (估计服务器现在 − move_start_ms) / 1000
```

- 只用服务器时间轴；不要用本机 `realtimeSinceStartup` 直接减 `move_start_ms`。
- 对时差只在新 AOI 帧到达时更新。
- 丢包：沿旧速度滑；下一帧 `position` **直接覆盖**，不做多秒平滑拉回。

---

## 4. 导出 / 验收字段

压测导出请带（便于对照服务端 `aoi_move_emit_reason`）：

- `aoi_move_events` / `aoi_move_per_sec`（本轮 P0 已有）
- `aoi_move_with_start_ms`、`aoi_move_start_ms_nonzero_ratio`（外推开后应明显 &gt;0）
- 可选：观察端 stall / `sample_age`

服务端灰度时看：`vel_changed` / `keyframe` / `arrival_stop` 为主，`interval`/`drifted` 应下降。

---

## 5. 联调顺序（建议）

1. 客户端：**外推默认开**（与服务端默认一致；schema 已对齐）  
2. 服务端：C1…C1g 已合入，**外推 + enforce 默认开**（`=0` 可关）  
3. 905 验收见 `p1g_extrapolate_acceptance_905_20261117.md`（timeout 1.68% 优于 P0）  
4. 约束：一边关外推时另一边不要单开  

---

## 6. 服务端现状（给客户端同学）

- P0 enforce + P1 外推为正式默认；P1g@905 timeout **1.68%**。  
- 误差用**呈现坐标**；远关键帧默认 10s（`GAMEMESH_AOI_FAR_KEYFRAME_MS`）。  
- ENTER/MOVE 在外推开启时由 `FillEntitySnapshot` / `FillMoveSnapshot` 写入 `vx/vz/move_start_ms`。  
