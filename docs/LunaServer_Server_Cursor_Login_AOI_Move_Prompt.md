# LunaServer 服务器基础版收口 Cursor 提示词

> 目标：Unity 客户端可以登录、主动下线；两个真实 Unity 客户端进入同一公共地图后能互相看到；任一客户端移动时另一端能收到并显示位置变化。
>
> 服务器仓库：`https://github.com/madongxin/webserver`
>
> 本次审计服务器 HEAD：`17912f2033344ee579fa388ba8f7467e1790f772`
>
> 本次审计 Unity HEAD：`2e43ed53267887614a358116d42b0de9c19b6822`

## 你的角色

你正在修改 LunaServer C++ 服务器仓库。请先完整阅读本文件、仓库根目录 `AGENTS.md`、`docs/release/server-client-foundation-status.md`，再检查代码。不要根据旧状态文档声称完成；所有结论必须以当前 HEAD 和本轮实际测试为准。

客户端和服务器是两个独立 Git 仓库。可以在同一个 Cursor multi-root workspace 中修改，但必须分别检查、分别测试、分别提交。未经用户明确授权，不要 commit、push、合并或创建 PR。

## 不可违反的规则

1. 保留用户已有未提交修改；先执行 `git status --short --branch`，工作区不干净时先说明重叠风险。
2. `webserver/proto/game.proto` 是公网协议唯一事实源。客户端不得反向覆盖服务器协议。
3. 本轮不要继续增加玩法协议；先冻结并发布当前协议。
4. 不允许用 `LUNASERVER_ALLOW_LEGACY_NO_HELLO=1`、跳过 schema 校验或伪造 PASS 绕过不兼容。
5. 不重写 Reactor，不引入 gRPC，不让 GameLogic 接触密码或登录凭据。
6. `MarkDisconnected` 不等于 `Logout`；主动登出必须释放 Session、Logic 绑定和 AOI 实体。
7. 任何 `NOT RUN`、缺依赖、缺 Unity 二进制或缺 live Gateway 都是 `BLOCKED`，不能写成 PASS。
8. 本次验收只把登录、主动下线、双客户端互见、移动同步列为 P0。邮件、聊天、好友不是本轮发布阻塞项。

## 已审计事实

### 当前已经存在，不要重做

- 正式 TCP Gateway、`ClientHello`、Heartbeat、注册、登录、Reconnect 已有真实路由。
- 公共地图 1001、50 人容量、EnterMap、AOI Enter/Move/Leave 已有实现。
- `scripts/test_two_player_aoi.sh` 已要求：
  - `login_ok=1`
  - `same_instance=1`
  - 双方 AOI Enter
  - `aoi_move_ok=1`
  - `logout_ok=1`
  - `b_aoi_leave_on_logout=1`
  - `session_released=1`
- 当前地图文件一致：客户端与服务器地图 JSON SHA-256 都是 `ceef56586c5281dca4ce45340f511d0d577fd724b14131ae5a21d01ea7f41317`。
- 服务器状态文档记录过 C++ TCP 双玩家测试通过，但这是历史证据；必须在最终工作树重新执行。

### 当前阻塞和风险

1. 服务器 `proto/game.proto` SHA-256：

   ```text
   f16462b65fa998a1c1d63be4710b2be927c9ec1b8ef47756803b12798d6e8665
   ```

   Unity 当前 schema SHA-256：

   ```text
   4c29a73aa7fbed19f122e122bc1832852e593f6bfaca0b7433249391e2ec643d
   ```

   Unity manifest 仍绑定服务器 `60542e5`。正式 Hello 会因 schema 不一致拒绝业务请求。

2. 服务器最新协议比客户端多：
   - `MapManifestEntry`
   - `ServerHelloRsp.gameplay_config_version = 12`
   - `ServerHelloRsp.map_manifest_version = 13`
   - `ServerHelloRsp.maps = 14`
   - `SessionReplacedNotify`
   - `GameResponse.session_replaced = 76`

3. `OrchestrateGatewayLogout` 的返回值当前主要表示“响应是否编码成功”，调用处却把它当成“登出业务成功”并据此 `ForgetBind`。同时函数内部也会无条件 `GatewayConnRegistry::Forget(connection_id)`；当 Session Logout 失败时可能遗留服务端在线 Session，却丢失 Gateway 绑定。

4. `OrchestrateGatewayLogout` 调用 Logic `UnbindPlayer` 后没有检查 RPC 及业务结果。不能在 Logic 实体未移除时向客户端报告权威 Logout 成功。

5. 服务器状态文档提到 `scripts/test_unity_two_clients.sh`，但该文件实际不存在。

6. `.github/workflows/ci.yml` 的 full job 调用强制要求 `LUNA_REPO` 的 `client_ready_gate.sh`，却没有 checkout Luna 或设置 `LUNA_REPO`。当前流水线配置无法完成这条门禁。

## 完成定义

只有同时满足以下条件，才可以写“基础版服务器完成”：

| 能力 | 必须证据 |
| --- | --- |
| 协议兼容 | 两端 `game.proto` 字节一致，hash 为 `f16462b6…`，Unity manifest `source_commit` 指向当前服务器兼容提交 |
| 登录 | 正式 Hello 后，两个不同账号都收到成功 LoginRsp，并进入同一个非零地图实例 |
| 互相可见 | A 收到 B 的 AOI Enter，B 收到 A 的 AOI Enter；客户端场景中也有远端实体 |
| 移动同步 | A 移动后 B 收到更新后的坐标和递增 `state_seq`；至少再验证一次 B→A |
| 主动下线 | LogoutRsp 成功；Session 离线；Logic/AOI 实体删除；另一客户端收到 Leave；断开连接不会把 Session 恢复成 DISCONNECTED/ONLINE |
| 自动化 | C++ TCP 门禁与真实双 Unity 二进制门禁均真实执行并 PASS |

## 阶段 S0：冻结并发布当前公网协议

先完成本阶段，失败时停止，不进入 S1。

1. 验证 `proto/game.proto`、`game/game.pb.h/.cc` 与导出 descriptor 一致。禁止手工编辑生成文件。
2. 使用 `scripts/export_unity_protocol.sh` 导出到临时目录，确保产物包含：
   - `game.proto`
   - descriptor
   - schema/descriptor SHA-256
   - `protocol_manifest.json`
   - 当前服务器完整 commit ID
   - required types，其中必须包含 `MapManifestEntry` 和 `SessionReplacedNotify`
3. 不覆盖已冻结的 published v1 descriptor；如当前协议属于兼容追加，按仓库既有 append-only 规则记录。
4. 等客户端从本次导出物重新生成 C# 后执行：

   ```bash
   LUNA_REPO=/absolute/path/to/luna ./scripts/check_luna_protocol_contract.sh
   LUNA_REPO=/absolute/path/to/luna ./scripts/test_unity_contract.sh
   ```

5. 检查器必须同时校验：
   - server/client schema hash
   - 客户端 manifest schema hash
   - manifest `source_commit`
   - descriptor hash
   - required message/oneof case
6. 更新 `docs/release/server-client-foundation-status.md`，删除旧的“current HEAD=60542e5/dirty tree”表述，写入实际 HEAD、客户端兼容提交和本轮命令结果。

S0 停止条件：任何 hash、descriptor、source commit 不一致，立即停止并输出精确差异。

## 阶段 S1：修复主动 Logout 的权威语义

重点检查：

- `runtime/brpc/GatewayLoginOrchestrator.cpp`
- `game/GameTcpGateway.cpp`
- `game/GatewayConnRegistry.*`
- `game/SessionStore.*`
- Logic 的 `UnbindPlayer` 实现与返回结构
- `client/game_tcp_e2e_client.cpp`

要求：

1. 区分三类结果，不再用一个 `bool` 混用：
   - 请求/响应是否成功解析编码；
   - Session Logout 是否成功或幂等地已离线；
   - Logic Unbind/AOI Leave 是否成功。
2. 对已绑定连接，完全忽略客户端自报 player/session/fence，使用 Gateway 权威绑定。
3. 只有权威清理达到定义的成功状态后，才向客户端返回 `LogoutRsp.ok=true` 并移除连接绑定。
4. 若 Logic Unbind 成功而 Session Logout 暂时失败，必须有可重试/补偿路径，且客户端收到明确错误；不能假装成功。
5. 若 Session 已离线，重复 Logout 应幂等成功，但仍要确保 Logic/AOI 不残留。
6. 主动 Logout 成功后，TCP disconnect callback 不得再执行会把 Session 改成 `DISCONNECTED` 的旧绑定清理。
7. 不因清理失败泄露 token/fence；日志只记录脱敏的 player、gateway、logic、map、trace/error code。

必须增加或强化测试：

- 正常 Logout：Rsp 成功、Session released、Logic unbound、对方收到 AOI Leave。
- 重复 Logout：幂等成功，无重复实体/重复 Leave。
- Session Redis Logout 失败：Rsp 失败，不能错误 `ForgetBind`，有重试或断线补偿。
- Logic Unbind 失败：不能返回完整成功，恢复后可收敛，无幽灵实体。
- Logout 与 TCP close 并发：最终 Session 离线、实体为 0、无死锁、无 use-after-free。

S1 停止条件：只要存在“Rsp 成功但 Session/Logic/AOI 任一未清理”，本阶段失败。

## 阶段 S2：保留并强化 AOI/移动目标链路

不要重写现有 AOI。以测试驱动方式核对：

1. 两个不同账号经两个 Gateway 登录并进入地图 1001，必须分配到同一非零公共地图实例。
2. 双方 Enter 事件必须携带对方正确的 `player_id/entity_id/name/position/state_seq/map_instance_id`。
3. Move 只接受当前绑定身份、当前地图实例和合法速度/坐标；服务端覆盖客户端伪造 player_id。
4. A 移动后，B 收到新坐标；B 移动后，A 收到新坐标；`state_seq` 单调递增。
5. A Logout 后 B 收到 Leave，之后再收 A 的旧 Move 必须丢弃。
6. 继续保留 50+1 容量门禁，但不让它阻塞最小双玩家调试脚本的定位信息。
7. 测试输出结构化字段：两个 player_id、map_instance_id、初始/最终位置、state_seq、Enter/Move/Leave 断言。

## 阶段 S3：建立真实 Unity 双客户端门禁

1. 新增与状态文档一致的 `scripts/test_unity_two_clients.sh`。
2. 脚本必须显式要求：
   - `LUNA_REPO`
   - `UNITY_CLIENT_BIN`
   - live Gateway host/port，或由脚本启动的本地正式集群
3. 先运行跨仓库协议检查，再启动/确认集群健康，最后调用 Luna 的目标专用双客户端脚本。
4. Unity 门禁只验证本轮目标：Hello、登录、同图、双方互见、双向移动、A 下线后 B 收到 Leave、B 下线。邮件/聊天另设扩展门禁，不得绑死基础门禁。
5. 缺二进制、缺 Gateway、超时、任一客户端非零退出均为 `BLOCKED/FAIL`，不得 PASS。
6. 归档双方 `result.json`、`events.jsonl`、Player.log、服务器日志和协议/commit 元数据。
7. 修复 GitHub Actions：显式 checkout Luna 到固定目录并设置 `LUNA_REPO`，或把跨仓库/Unity 门禁拆成有明确输入的独立 workflow。不能保留“强制要求但永远不提供”的配置。
8. 若 CI 要运行 Unity E2E，必须下载前一作业构建出的客户端 artifact，并配置 live Gateway；不能在只有源码的 job 中假定二进制存在。

## 必须执行的验证

按环境能力执行；缺依赖要报告，不得删除测试或降低断言：

```bash
git status --short --branch
git rev-parse HEAD
bash -n scripts/*.sh
./scripts/check_public_protocol.sh
./scripts/check_deps.sh --full
./scripts/build.sh Debug
./scripts/test.sh unit
./scripts/test_reactor.sh
./scripts/test_integration.sh
LUNA_REPO=/absolute/path/to/luna ./scripts/check_luna_protocol_contract.sh
LUNA_REPO=/absolute/path/to/luna ./scripts/test_unity_contract.sh
./scripts/test_hello_heartbeat.sh
./scripts/test_two_player_aoi.sh
./scripts/test_map_capacity.sh
LUNA_REPO=/absolute/path/to/luna \
UNITY_CLIENT_BIN=/absolute/path/to/LunaServerClient \
./scripts/test_unity_two_clients.sh
git diff --check
git status --short --branch
```

如果服务器依赖、Redis、MySQL、brpc 或 Unity 二进制缺失，输出 `BLOCKED`、缺失项和用户下一步命令。

## 最终输出格式

完成后按以下格式报告：

1. `Server baseline`：开始/结束 commit、worktree 状态、协议与地图 hash。
2. `Changed files`：逐文件说明修改理由。
3. `Target verdicts`：Login、Logout、AOI visibility、Move sync 分别为 PASS/FAIL/BLOCKED，并列证据。
4. `Tests run`：命令、退出码、关键断言、报告路径。
5. `Not run`：缺失环境或外部依赖。
6. `Client handoff`：客户端必须导入的服务器 commit、schema hash、descriptor hash。
7. 不要把历史报告当作本轮 PASS，不要自动提交。

