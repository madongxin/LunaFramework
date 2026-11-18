# 停滞连接取证

构建基线是 `4d0e649639d56990a44b67bca42422f5e955d329`。这一轮的指标和脚本还在工作区，没有提交。下面凡是写「上次」的，都是 2026-11-13 10:38:18 起到 10:59:22 的那次抓取，环境变量原值是 `LUNASERVER_FORMAL=1`、`LUNASERVER_EGRESS_BUDGET_MBPS=2.4`，`LUNASERVER_AOI_TRACE`、`LUNASERVER_AOI_V2_*`、`LUNASERVER_AOI_MEMBERSHIP_UNIFIED`、`LUNASERVER_SENDBUF_V2`、`LUNASERVER_READ_BACKPRESSURE` 都没有出现在进程环境里。这一轮没有重跑压测，所以新指标的线上值写「无数据」。

这一轮只加计数、采样、只读管理接口、脚本、文档和单测。发送、AOI 成员、2.4 Mbps、心跳、2 秒移动超时、慢客户端 10/30/90 秒都没有改。上面那几个开关的默认值也没有改。

## 1. 现有 unsent16 采样器

`AoiObserveBusySocket`（`runtime/AoiEvidence.cpp`）在网关连接的 EventLoop 上被调用，调用点是移动帧写出之后，以及回包即将写入之前。应用缓冲字节加上内核未发送字节超过 16KB 才读 `TCP_INFO`。同一条连接两次读取至少间隔 1 秒（`kBusyMinIntervalMs`）。指标锁拿不到就跳过这一次，不在 EventLoop 上等。

分类在读到 `TCP_INFO` 之后：`tcpi_retrans` 或 `tcpi_retransmits` 大于 0 记 `retrans`；否则 `tcpi_snd_cwnd` 在 1 和 2 之间记 `cwnd_low`；其余记 `quiet_tcp`。`with_enter` / `with_snapshot` 表示这一秒里本进程已经接受过对应帧，不是「这一条连接的未发送队列里有 ENTER」。分位数向量最多留 2048 个样本，样本是「一次 TCP 读取」，不是「一条连接」。每个分位数现在带 `_count`，那个数就是向量里的样本数。

`ParkProbe` 是另一条采样：只在位置帧因为内核未发送超过 16KB 而停在应用缓冲里时，每秒最多读一次 `TCP_INFO`。一条既停放又被 unsent16 看到的连接，两套采样加起来大约每秒两次。

## 2. 为什么上次 notsent 一边无数据、一边有值

本机编译用的 `linux/tcp.h` 里的 `tcp_info` 没有 `tcpi_notsent_bytes`，也没有 `tcpi_bytes_retrans`。`netinet/tcp.h` 更短，到 `tcpi_total_retrans` 为止。

`lunaserver_unsent16_notsent_bytes` 从来不是那个结构体字段。它是 `SIOCOUTQNSD`（ioctl `0x894B`）读到的「还没交给网卡的字节」。所以上次分位数能停在 17088–17100，而名叫 `tcpi_notsent_bytes` 的那一列是无数据。这次管理接口里的 `notsent_bytes` 同样来自这个 ioctl，`notsent_source` 写成 `SIOCOUTQNSD`。ioctl 失败时 JSON 里是 `null`，Prometheus 不输出这条分位数。

`tcpi_bytes_acked` 在 `linux/tcp.h` 里有。本机用这个头对一个刚连上的套接字做 `getsockopt(TCP_INFO)`，返回的长度等于结构体大小 144，`tcpi_bytes_acked` 读到 1。旧采样器没有读它，所以上次报告里它是无数据。这轮开始读，并记录相邻两次采样的增量。第一次采样还没有上一笔，增量是 `null`，不记成 0。

`tcpi_bytes_retrans` 头文件里没有，不编造。替代口径是 `tcpi_total_retrans` 的增量，单位是段，不是字节，序列名叫 `lunaserver_unsent16_tcpi_total_retrans_delta`。

## 3. 停滞连接指标和管理接口

| 指标 | 类型 | 口径 |
| --- | --- | --- |
| `lunaserver_stalled_conns` | gauge，单位 connections | 当前应用缓冲加内核未发送超过 16KB 的连接数。掉回去之后从这里面拿掉 |
| `lunaserver_stalled_conn_age_ms` | 分位数，单位 milliseconds，带 `_count` | 抓取时每条仍停滞的连接已经持续了多久。`_count` 是连接数，不是 TCP 采样次数 |
| `lunaserver_stalled_conn_total` | counter，单位 connections | 进入过该状态的不同连接数，上限 8192。同一条连接恢复后再停滞不重复加 |

`GET /conn/stalled?n=20` 挂在现有 HTTP 管理口上。默认绑定仍是 `127.0.0.1`（`LUNASERVER_HTTP_BIND` 不改）。只读。JSON 在处理这次 HTTP 请求的线程上拼，不在连接的 EventLoop 上拼。对端地址在连接第一次越过 16KB 时用 `getpeername` 存成地址结构，格式化留到管理线程。玩家 id 也在管理线程上查连接表。

字段：`conn_id`、`player_id`、`peer`、`unsent_bytes`、`app_bytes`、`tcpi_unacked`、`tcpi_retrans`、`tcpi_backoff`、`tcpi_rto_us`、`tcpi_rtt_us`、`tcpi_snd_cwnd`、`tcpi_last_ack_recv_ms`、`tcpi_last_data_sent_ms`、`tcpi_bytes_acked_delta`、`tcpi_bytes_retrans_delta`、`tcpi_total_retrans_delta`、`notsent_bytes`、`age_ms`、`write_fail`，以及 `bytes` 里 move/reply/enter/leave/other 的字节和占比。内核没给的字段是 `null`。

`lunaserver_write_fail_total{frame,reason}` 现在计数为 0 也输出。看不到某一行，才是这个进程没有导出这个指标。

`lunaserver_write_fail_per_conn` 的一个样本是「已经至少失败过一次的一条连接」上的累计失败次数。失败次数一直是 0 的连接不在这个分布里，所以 p50 不会被大量 0 拉低。`lunaserver_write_fail_per_conn_count` 是这个分布里的连接数。一条连接都没失败过时，这个 `_count` 是 0，分位数行不出现。

## 4. 两个 LEAVE 失败数为什么对不上

这是从代码读出来的，不是这次新的实测。

网关 `lunaserver_write_fail_total{frame="leave"}` 在 `AoiNoteFrameWrite` 里加。一次没写进发送缓冲的帧，里面每个 `op=3` 的事件加 1。原因包括 `queue_full`、`timed_wait`、`closed`。`timed_wait` 是入队等了 200ms 还没看到结果，回调之后仍可能写进去。所以一次逻辑上的 LEAVE，网关可以既记一次超时，又在真正写失败时再记一次。

逻辑服 `lunaserver_aoi_leave_write_fail_total` 在 `MapRuntime::NoteAoiWrite(op=3, ok=false)` 里加，一次是「一个观察者、一个实体、一次网关回来说没收下」。它按观察者-实体对记 pending，重复失败不增加 pending 行数，但失败计数会加。逻辑服进程和网关进程的计数器各算各的。

上次网关 leave 的 queue_full 是 220+1163，逻辑服 write_fail 是 632+1187。两个数对不上，和上面的口径差一致。差多少是重试、超时后补写、还是一帧里多个事件，这次没有新的拆分数据。

## 5. 预算扣了哪些帧

从代码读，没有改。

`AoiMoveCoalesceLedger::Pick` 和 `NoteKernelWrite` 只处理这条连接合并槽里的 MOVE。`NoteKernelWrite` 在 `frame_bytes > 0` 时把 `frame_bytes + kOnWireHeaderBytes` 加进这一秒的预算。`kOnWireHeaderBytes` 在 `runtime/AoiMoveCoalesceLedger.h`，值是 66：以太网 14 + IPv4 20 + TCP 20 + 12 字节选项余量。

网关在 `FlushMoveSlot` 里把 `EgressBudgetBytesPerSec() / 在线连接数` 传给 `Pick`。`EgressBudgetBytesPerSec` 用 `LUNASERVER_EGRESS_BUDGET_MBPS`（默认 2.4）和 `LUNASERVER_GATEWAY_INSTANCES`（默认 2），得到每网关 150000 字节/秒。`SetEnforceByteBudget` 只有 `LUNASERVER_AOI_V2_LAYERS=1` 才为真，默认关。默认关时，已经超过约 200ms 没写出的 MOVE 可以超过字节余量被选中。预算数字仍然只统计这些 MOVE。

reply、ENTER、LEAVE、心跳、全量快照不经过 `NoteKernelWrite`，不扣这份每连接预算。它们走回包或可靠急送。`lunaserver_egress_wire_bytes_estimate{frame}` 是另一份记账：每接受一帧，加上 payload 和同一个 66 字节常量。它不参与限流。

## 6. LEAVE pending

`lunaserver_aoi_leave_write_pending` 是 `g_leave_fail_since` 的行数，一行是一个观察者加一个实体。逻辑服进程会把它拆成 `lunaserver_aoi_leave_write_pending_viewer{viewer="online"|"offline"}`，并在还有 pending 时输出 `lunaserver_aoi_leave_write_pending_oldest_ms`。没有 pending 时不输出年龄，避免把「没有最老的一条」写成 0。网关进程不知道观察者是否在线，不输出拆分。

单测 `leave_pending_audit_test` 当前失败，按要求留着，没有改清槽逻辑。

1. 观察者断线后，一轮 `AdvancePresented` 没有清掉该观察者的 pending。命中 `MapRuntime::RemovePlayerInterest`：它删掉 `leave_again` 里这个观察者，不调用 `AoiNoteLeaveGiveUp`。`RetryPendingLeaves` 发现观察者已断线时把 `leave_again` 丢掉，同样不调用 `AoiNoteLeaveGiveUp`。`g_leave_fail_since` 只在 `AoiNoteLeaveWriteOk` 和 `AoiNoteLeaveGiveUp` 里删除。
2. N 个观察者、M 个实体的上界不成立。同一对重复失败不会加行，但任意新的观察者-实体对都会插入，没有上限。命中 `AoiNoteLeaveWriteFail`。

`scripts/test_unit.sh` 不跑这个失败用例。直接跑 `build/test/leave_pending_audit_test` 会以退出码 1 结束，并打印上面的位置。

## 7. /aoi/visible 上次为什么是空的

上次抓取写在 `run/formal/aoi_retest_20261113_1059.md`：10:59:22 请求的是逻辑服 `127.0.0.1:8090/aoi/visible` 和 `8091/aoi/visible`，正文都是 `{"viewers":[]}`。集体下线在 10:54–10:55。这是下线之后抓的。

接口读的是本进程 `MapRuntime::CopyVisibleSets`，也就是当前 `visible_targets`。断线会把观察者从这份表里拿掉。下线之后空列表和这个实现一致。压测还在进行时这份列表里有没有人，没有当时的抓取，写无数据。网关进程的同一路径也会是空的，因为玩家兴趣在逻辑服。对账要打逻辑服，并且要在下线之前打。

`scripts/aoi_reconcile.py` 接受两份 JSON。服务端是 `/aoi/visible` 的原文。机器人文件可以是同一种 `{"viewers":[{"id":819,"entities":[1,2]}]}`，也可以是 `{"819":[1,2]}`。`notes.unknown_move`、`notes.move_after_leave`、`notes.leave_distance` 有就打印，没有就写无数据。输出幽灵总数、缺失总数、按观察者的幽灵数直方图，以及差额最大的前 10 个观察者。

## 8. 给机器人维护者的最小改动

本仓库没有机器人代码。压测结束前、集体下线前，导出模型里的实体 id。文件格式见上一节。另外记三个数，放进 `notes`：

- 收到一个模型里还没有的实体的 MOVE 的次数。
- 收到某实体的 LEAVE 之后，又收到该实体 MOVE 的次数。
- 收到 LEAVE 时，该实体最后已知距离（米），一条 LEAVE 一个数。

请写明：收到未知实体的 MOVE 时，机器人是创建模型，还是忽略。这次没有这段代码，结论是无数据。

## 9. 只给前 N 个观察者开 trace

`LUNASERVER_AOI_TRACE` 默认仍关。关着的时候热路径不解析。要采样时只在逻辑服上设：

```bash
LUNASERVER_AOI_TRACE=1
LUNASERVER_AOI_TRACE_MAX_VIEWERS=8
```

`MAX_VIEWERS` 记下最先出现的 8 个观察者 id，其余观察者的事件丢掉。不要在两台逻辑服上都不设这个上限。网关不用开。查 `GET /aoi/trace?viewer=<id>&n=50`。

## 10. 出口字节

`lunaserver_egress_wire_bytes_estimate{frame}` 和 `lunaserver_tx_frames_by_frame{frame}`、`lunaserver_tx_bytes_by_frame{frame}` 一起从 `/metrics` 读。这轮没有新的压测，三列速率都是无数据。

上次整窗口 payload 平均约 1.2 Mbps，平均 payload 约 82 字节。若每一帧单独占一个 TCP 段，估算线上字节约是 82+66，速率约 `1.2 * 148 / 82 ≈ 2.17` Mbps。这是用上次的 payload 和代码里的 66 推的，不是网卡，也不含重传。一段里有几帧，上次无数据。重传会再抬高网卡字节。所以不能用这个数判断有没有超过 2.4 Mbps。

`scripts/egress_sampler.sh` 每 10 秒追加一行 CSV。`scripts/egress_report.py` 打出整窗口和最忙 60 秒的三列：payload Mbps、估算线上 Mbps、网卡 tx Mbps，以及平均每个 `TcpOutSegs` 增量里有几帧。采样用 `nstat -a`，不用 `-z`，因为 `-z` 会把内核计数清掉。

`scripts/dump_parked_sockets.sh --watch` 由 `start_formal.sh` 拉起，停集群时跟着停。某条游戏口连接的 Send-Q 连续 5 秒超过 16KB，就存 `ss -tinm` 原文、全体连接的 Send-Q/Recv-Q，以及那三个 TCP 计数。同一对端 30 秒内只存一次。不 ssh 到压测机。压测机上的命令在 `scripts/dump_client_side.md`，路径确认在 `scripts/path_topology_check.md`。

## 11. H1–H5

| 假设 | 现在能从代码说到哪 | 还要等复压 |
| --- | --- | --- |
| H1 堆积来自少数连接的 TCP 停滞 | 采样和预算都是按连接记的。上次 unsent16 里 retrans 占 88%、cwnd p50 为 2、rto p50 约 40 秒，和「单连接重传」一致。代码不能证明这不是全局产出超过网卡 | `lunaserver_stalled_conns`、`/conn/stalled`、`ss -tinm`、压测机 Recv-Q、网卡 tx 丢包。这轮全是无数据 |
| H2 线上字节可能高于 payload 口径 | 每接受一帧的估算是 payload+66。用上次 1.2 Mbps 和 82 字节帧粗算约 2.17 Mbps，前提是一帧一段、没有重传 | 采样 CSV 的三列和「每段几帧」。这轮无数据 |
| H3 reply、ENTER、LEAVE、心跳、全量快照不扣每连接位置预算 | 成立。只有合并槽里的 MOVE 进入 `NoteKernelWrite`。默认不强制字节上限 | 不需要为了这个假设再压测。这些帧占了多少出口，等第 10 节的分帧字节 |
| H4 观察者断线后 pending 没有及时清掉 | 单测失败，位置在第 6 节。证据表和 `leave_again` 不是同一张表。上次下线约 3 分钟后 pending 仍有 90，和这张表不清掉一致 | 复压后看 `pending_viewer{viewer="offline"}` 和 `pending_oldest_ms` 是否在下线后还在涨 |
| H5 网关 write_fail 和逻辑服 leave write_fail 不是同一个数 | 成立，口径在第 4 节 | 两边的分原因计数（含 0 值序列）和 `/conn/stalled` 里的 `write_fail`。上次的差具体拆开仍是无数据 |

## 12. 复压时要留下的东西

压测前把下面的原值记进报告，不要改它们：

```text
LUNASERVER_FORMAL=1
LUNASERVER_EGRESS_BUDGET_MBPS=2.4
LUNASERVER_GATEWAY_INSTANCES=2
LUNASERVER_HTTP_BIND=127.0.0.1
```

不要设置 `LUNASERVER_SENDBUF_V2`、`LUNASERVER_READ_BACKPRESSURE`、`LUNASERVER_AOI_V2_LAYERS`、其它 `LUNASERVER_AOI_V2_*`、`LUNASERVER_AOI_MEMBERSHIP_UNIFIED`。只有要做第 9 节的 trace 时，才在逻辑服上加 `LUNASERVER_AOI_TRACE=1` 和 `LUNASERVER_AOI_TRACE_MAX_VIEWERS=8`。

压测中：

- `start_formal.sh` 已经在抓 `run/formal/parked-ss/`。
- `./scripts/egress_sampler.sh 0 run/formal/egress_sample.csv <网卡>` 跑满整个窗口。
- 人还在线时保存两台逻辑服的 `/aoi/visible`。
- `curl -s 'http://127.0.0.1:8080/conn/stalled?n=20'` 和 `8082` 同样一份。
- 压测机按 `scripts/dump_client_side.md` 和 `scripts/path_topology_check.md` 留原文。

压测后：

- `python3 scripts/egress_report.py run/formal/egress_sample.csv`
- `run/formal/parked-ss/`、`run/formal/logs/parked-ss.log`
- 两台网关和两台逻辑服的 `/metrics`，以及 `/proc/<pid>/environ`
- 机器人导出的实体列表
- 构建 SHA 用 `git rev-parse HEAD`，并注明工作区有没有未提交改动
