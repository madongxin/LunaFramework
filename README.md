
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
