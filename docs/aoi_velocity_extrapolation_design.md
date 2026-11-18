# 速度向量外推（设计，未改协议，未改代码）

状态：已确认。`EntitySnapshot` 已增加 `vx=8`、`vz=9`、`move_start_ms=10`。`LUNASERVER_AOI_EXTRAPOLATE` 默认关。

## 要解决的事

观察者看到的别人位置，今天来自 `AoiEvent.op=2` 的坐标快照。人一直走时，每个观察者都要反复收同一条轨迹。出口预算默认 2.4 Mbps，分到两条 Gateway 之后每条连接每秒只够更新很少几个实体。外推是让客户端用速度自己把人沿直线往前放，服务器只在速度变化、进视野、或误差过大时再发一帧。

## 协议字段

加在 `EntitySnapshot` 末尾，旧编号不动：

| 字段 | 编号 | 单位 | 含义 |
|---|---|---|---|
| `vx` | 8 | 米/秒 | 水平速度，X |
| `vz` | 9 | 米/秒 | 水平速度，Z |
| `move_start_ms` | 10 | 毫秒 | 这组速度开始生效的服务器时间 |

`AoiEvent.server_time_ms`（字段 3）保持现状：MOVE 用服务器确认这次位移的时间，ENTER/LEAVE 用发出时间。`move_start_ms` 是速度段的起点，可以早于本帧的 `server_time_ms`（补发关键帧时速度没变，起点仍是转向那一刻）。

`MoveReq` / `MoveRsp` 不加这些字段。上行仍然只报目标点。速度由服务器用连续两次权威坐标和 `server_time_ms` 计算。

## 兼容

proto3 里没填的字段解码成 0。新服务器发给旧客户端时，旧代码不读 8/9/10，仍用 `position` 摆人。

本仓库 Hello 会核对 `schema_sha256`。字段一加，摘要就变，旧客户端会被 `ERR_SCHEMA_MISMATCH` 拒绝，做不到「不改客户端也能连上」。上线时要二选一，写进同一开关：

- 默认关：不写新字段，schema 与现在的 `edfd9fb92b1961dac7451d0b78be4e12ef9ee1424d9716d7e02dbcd5d237c6ae` 一致。
- 打开后：schema 换成新摘要，客户端必须同时升级。不提供「旧摘要也能进、但悄悄带新字段」的双摘要，避免两套 AOI 语义混在一条连接上。

## 何时广播

只对 `op=2` 的 MOVE，以及 `op=1` 的 ENTER。LEAVE 不带速度。

1. 速度向量相对上次已广播的 `(vx, vz)` 发生变化（含从走到停、从停到走、转向）。立即广播，并把 `move_start_ms` 设为这次确认的 `server_time_ms`。
2. 正在移动（速度不是 0）的实体，距上次发给这个观察者超过 `LUNASERVER_EXTRAP_KEYFRAME_MS`（默认 4000，允许 3000–5000）时补一帧。`move_start_ms` 不变，`position` 是服务器此刻的权威点。
3. 速度为 0 的实体不补关键帧。
4. 观察者与目标水平距离大于 16 米时，不做第 2 条的周期关键帧。速度变化和进视野仍然发。
5. 新进入视野走现有 ENTER（`op=1`），`EntitySnapshot` 带上当前 `vx, vz, move_start_ms` 和权威 `position`。不另造消息。

## 外推误差

服务器为每个观察者、每个目标保存上次广播时的位置、速度和 `move_start_ms`。广播决策时计算：

```
预测 = 上次位置 + (vx, vz) * (now - move_start_ms) / 1000
误差 = 预测与当前权威 (x, z) 的水平距离
```

误差大于 `LUNASERVER_EXTRAP_ERROR_M`（默认 1 米）才补发。补发后更新保存的位置和 `move_start_ms`。阈值是配置项。

这个误差是服务器自己算的，不依赖客户端回传。公网丢包时服务器仍按自己的时钟补发，客户端没收到就继续用旧速度，直到下一帧或 ENTER。

## 时钟

外推只用服务器时间，不用 `MoveReq.client_time_ms`。

客户端对时：收到带 `server_time_ms` 的 AOI 帧时，记下 `server_time_ms` 与本地接收时刻的差。外推用 `估计的服务器现在 = 本地现在 + 这个差`。差只在新帧到达时更新，不在两帧之间漂移修正。RTO 很长时，人会沿旧速度多走一段，下一帧用权威 `position` 直接拉回，不做多秒的平滑。

`move_start_ms` 与 `server_time_ms` 同一时钟。客户端不要用本机 `Time.realtimeSinceStartup` 直接减 `move_start_ms`。

## 丢包

- 丢的是速度变化帧：客户端沿旧速度走，直到误差补发或下一个关键帧。关键帧的 `position` 是权威点，收到就覆盖本地外推。
- 丢的是关键帧：下一帧（速度变化、下一次关键帧、或重新 ENTER）同样覆盖。
- 不重传 MOVE。MOVE 今天就不是可靠消息。ENTER/LEAVE 仍走现有可靠路径。
- `state_seq` 仍然每次服务器接受一次 Move 就加 1，包括坐标没变的接受。它表示权威姿态的代数，不是「广播序号」。观察者看到的 `state_seq` 可以跳号。跳号不表示中间帧丢了，也不触发全量快照。全量快照的条件维持现状（可靠序号缺口），不要因为 MOVE 的 `state_seq` 不连续就 `WorldSnapshot`。

## 服务端怎么接到现有槽上

`MapRuntime::AllowMoveAoi` 今天按距离做最小间隔（近 200 毫秒，远由 `LUNASERVER_FAR_MOVE_INTERVAL_MS` 控制）。外推打开后，这里改为上面的五条规则，而不是再套一层时间间隔。

Gateway 的 `AoiMoveCoalesceLedger` 仍然一条连接、一个实体只留最新状态。Offer 的内容从「最新坐标」变成「最新的位置 + 速度 + move_start_ms」。Pick 的预算字节数要重测：现在按每事件 38 字节估，加三个字段后单事件会变大，2.4 Mbps 的预算不能提高，能同时更新的实体数会变少。静默硬上限仍是近 `LUNASERVER_NEAR_SILENCE_MS`、远 `LUNASERVER_FAR_SILENCE_MS`，只决定谁先占预算，不提高预算。

已写入内核的字节不撤回。外推不改变 `TcpConnection` 的停放规则。

## Unity 要改什么

- Hello 使用新 schema。
- `op=2` 和 `op=1` 读取 `vx, vz, move_start_ms`。速度为 0 时把人放在 `position`，不再外推。
- 每帧本地位置 = `position + (vx, vz) * (估计的服务器现在 - 这一帧的 server_time_ms)`。`position` 是这一帧的权威点，`server_time_ms` 是这个点的时间。不要用 `move_start_ms` 当地点时间，否则关键帧会把已经走过的时间再加一次。`move_start_ms` 只表示当前速度从何时开始。
- 不要把 `state_seq` 空洞当成丢包去拉全量快照。
- 自己的角色仍以 `MoveRsp` 为准，不用外推自己。2 秒移动超时和心跳的处理不改。

## 开关与回滚

`LUNASERVER_AOI_EXTRAPOLATE` 默认 `0`。

- `0`：不写字段 8/9/10，按现在的坐标快照发 MOVE，schema 不变。
- `1`：按本文发。出问题就把环境变量设回 `0` 并重新构建带旧 schema 的二进制（字段留在 proto 里但服务器不填、Hello 仍报旧摘要做不到，所以回滚是整包回到打开开关之前的构建，不是热关字段）。

## 风险（未验证）

- 公网 RTT 约 1 秒、重传超时到几十秒时，关键帧间隔 3–5 秒会让人沿错误方向多走几米，下一帧被拉回。手感要在真实客户端上看，本文没有样本。
- 2.4 Mbps 不变。字段变大之后，同样预算能覆盖的实体数下降。远层不发周期关键帧就是为了把字节留给近处和速度变化。
- 转向很密时（每秒改一次速度）外推省不下字节，退回到今天的广播量。
