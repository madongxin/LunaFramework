# LunaFramework

**LunaServer** — C++17 分布式 MMO 游戏服务器（本仓库）。

客户端只连公网 Gateway VIP；登录由 Gateway 编排 Auth → Session → GameLogic；权威状态在 GameLogic / GameDB，不经客户端直连 Logic。

## 架构（MVP）

```
Client ──TCP──► VIP:8081 (L4) ──► Gateway ×2
                                    │
                    Auth.Login → Session.AcquireSession → GameLogic.BindPlayer
                                    │
              ┌─────────────────────┼─────────────────────┐
              ▼                     ▼                     ▼
         session 进程           GameLogic ×2          GameDB ×2
         (Auth+Session)         + GlobalService         → MySQL
              │                     │
              └──── Redis (token / fence / placement) ───┘
```

- **Gateway**：对外 Game TCP；HTTP 管理与 Push brpc 默认内网。
- **session**：同进程提供 AuthService 与 SessionService（proto 分离）；Auth 不创建 Session。
- **GameLogic**：玩法与 AOI；`PushBatch` 按 `gateway_instance_id` 定向推送，不广播。
- **GameDB**：账号/资产访问层（多实例 ≠ MySQL HA）。
- **GlobalService**（二进制 `globalservice`）：邮件等全局能力。
- 服务发现：etcd 可选；否则静态 `*_addrs` + `IServiceRegistry`。

细则：`docs/mmo-migration/topology-auth-session.md`、`AGENTS.md`。

## 技术栈

| 层 | 选型 |
|----|------|
| 语言 / 标准 | C++17 |
| 网络 | 自研 epoll Reactor（`tcp/`） |
| 客户端协议 | protobuf + ProtoFraming（`proto/game.proto`） |
| 服务间 RPC | brpc 1.9.0 |
| 会话 / Token | Redis（连接池 + Lua） |
| 持久化 | MySQL（GameDB） |
| 构建 | CMake；核心库目标 `lunaserver_core` |

## 角色二进制

构建产物在 `build/test/`：

| 二进制 | 角色 |
|--------|------|
| `gateway` | 边缘接入 |
| `session` | Auth + Session |
| `gamelogic` | 玩法 / AOI |
| `gamedb` | DB 访问层 |
| `globalservice` | GlobalService |

## 快速开始

```bash
# 依赖（brpc 等到 $HOME/.local/lunaserver-deps）
./scripts/check_deps.sh
# 缺依赖时：./scripts/install_deps.sh --build-brpc

./scripts/bootstrap_local_config.sh   # 本地配置
./scripts/build.sh Debug              # 或 Release；支持 clean

# 单角色冒烟
./build/test/session

# 正式多开（需 Redis / MySQL；公网 advertise 时设置）
# LUNASERVER_ADVERTISE_HOST=<VIP> ./scripts/start_formal.sh
```

测试：

```bash
./scripts/test_reactor.sh    # Reactor / ProtoFraming
./scripts/test_unit.sh
./scripts/test_all.sh
./scripts/test.sh unit       # 兼容旧入口
./scripts/test.sh integration
```

环境变量示例见 `.env.example`。正式模式 `LUNASERVER_FORMAL=1` 默认 fail-closed（拒绝随意公网 bind，除非显式允许）。

## 协议与客户端

- 公共协议：`proto/game.proto`（`csharp_namespace = LunaServer.Protocol`）
- 契约说明：`docs/client-protocol.md`、`docs/protocol/`
- Unity / Windows 测试客户端：`client/`

重新生成 C++ pb（改 `.proto` 后必做，勿手改二进制 descriptor）：

```bash
./scripts/gen_proto.sh
```

## 文档索引

| 文档 | 内容 |
|------|------|
| `AGENTS.md` | 拓扑约束与构建入口 |
| `docs/LunaServer_Cursor_All_Phases.md` | 全阶段路线图 |
| `docs/mmo-migration/` | 迁移状态、场景、数据归属 |
| `docs/runbook/` | 部署与故障 |
| `docs/release/` | 稳定版基线说明 |
| `docs/perf_audit/` | AOI / egress 性能审计 |

## 设计约束（请勿轻易打破）

- 不引入 gRPC，不拆独立 login 进程。
- 不重写 Reactor。
- GameLogic **不接受**密码/凭证（只走 Auth）。
- `MarkDisconnected` ≠ `Logout`。

## 仓库说明

| 名称 | 含义 |
|------|------|
| **LunaFramework** | Git 仓库 / 目录名 |
| **LunaServer** | 服务器产品与 CMake `project()` 名 |
