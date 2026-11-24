#pragma once

/**
 * @file SessionServiceImpl.h
 * @brief SessionService brpc 实现：AcquireSession / MarkDisconnected / Reconnect / Logout 等
 *
 * Auth 不创建 Session；本服务持 Redis fence（ONLINE/DISCONNECTED）。MarkDisconnected ≠ Logout。
 */
#include "session.pb.h"

class SessionServiceImpl : public sess::SessionService {
public:
    /** 登录后创建/绑定会话：写 Redis ONLINE + fence；Auth 不得调用本语义。 */
    void AcquireSession(::google::protobuf::RpcController *controller,
                        const ::sess::AcquireSessionRequest *request,
                        ::sess::AcquireSessionResponse *response,
                        ::google::protobuf::Closure *done) override;
    /** 断线标记 DISCONNECTED；≠ Logout，不清理账号登录态。 */
    void MarkDisconnectedV2(::google::protobuf::RpcController *controller,
                            const ::sess::MarkDisconnectedRequest *request,
                            ::sess::MarkDisconnectedResponse *response,
                            ::google::protobuf::Closure *done) override;
    /** 单 RPC 重连（兼容路径）；正式拓扑优先 Prepare/Commit 两阶段。 */
    void ReconnectV2(::google::protobuf::RpcController *controller,
                     const ::sess::ReconnectRequest *request,
                     ::sess::ReconnectResponse *response,
                     ::google::protobuf::Closure *done) override;
    /** 重连两阶段：准备（校验 fence/代次，预留新连接）。 */
    void PrepareReconnect(::google::protobuf::RpcController *controller,
                          const ::sess::PrepareReconnectRequest *request,
                          ::sess::PrepareReconnectResponse *response,
                          ::google::protobuf::Closure *done) override;
    /** 重连两阶段：提交（切换 ONLINE 绑定到新连接）。 */
    void CommitReconnect(::google::protobuf::RpcController *controller,
                         const ::sess::CommitReconnectRequest *request,
                         ::sess::CommitReconnectResponse *response,
                         ::google::protobuf::Closure *done) override;
    /** 重连两阶段：中止并回滚 Prepare 侧预留。 */
    void AbortReconnect(::google::protobuf::RpcController *controller,
                        const ::sess::AbortReconnectRequest *request,
                        ::sess::AbortReconnectResponse *response,
                        ::google::protobuf::Closure *done) override;
    /** 查询进行中的会话操作（重连/迁移）状态。 */
    void GetSessionOperation(::google::protobuf::RpcController *controller,
                             const ::sess::GetSessionOperationRequest *request,
                             ::sess::GetSessionOperationResponse *response,
                             ::google::protobuf::Closure *done) override;
    /** 主动登出：清理会话；与 MarkDisconnected 语义不同。 */
    void LogoutV2(::google::protobuf::RpcController *controller,
                  const ::sess::LogoutRequest *request, ::sess::LogoutResponse *response,
                  ::google::protobuf::Closure *done) override;
    /** 运维/顶号踢人：使会话失效并通知 Gateway。 */
    void Kick(::google::protobuf::RpcController *controller, const ::sess::KickRequest *request,
              ::sess::KickResponse *response, ::google::protobuf::Closure *done) override;
    /** 恢复被顶号前的会话（受控实验/运维路径）。 */
    void RestorePreviousSession(::google::protobuf::RpcController *controller,
                                const ::sess::RestorePreviousSessionRequest *request,
                                ::sess::RestorePreviousSessionResponse *response,
                                ::google::protobuf::Closure *done) override;
    /** 通知旧连接：会话已被新连接替换。 */
    void NotifySessionReplaced(::google::protobuf::RpcController *controller,
                               const ::sess::NotifySessionReplacedRequest *request,
                               ::sess::NotifySessionReplacedResponse *response,
                               ::google::protobuf::Closure *done) override;

    /** 解析或创建地图实例 Placement（分线/Owner）。 */
    void ResolveOrCreateMap(::google::protobuf::RpcController *controller,
                            const ::sess::ResolveOrCreateMapRequest *request,
                            ::sess::ResolveOrCreateMapResponse *response,
                            ::google::protobuf::Closure *done) override;
    /** 查询某模板当前分线列表。 */
    void QueryMapLines(::google::protobuf::RpcController *controller,
                       const ::sess::QueryMapLinesRequest *request,
                       ::sess::QueryMapLinesResponse *response,
                       ::google::protobuf::Closure *done) override;
    /** 创建副本实例 Placement。 */
    void CreateDungeon(::google::protobuf::RpcController *controller,
                       const ::sess::CreateDungeonRequest *request,
                       ::sess::CreateDungeonResponse *response,
                       ::google::protobuf::Closure *done) override;
    /** 玩家切线：更新路由到目标分线实例。 */
    void SwitchLine(::google::protobuf::RpcController *controller,
                    const ::sess::SwitchLineRequest *request, ::sess::SwitchLineResponse *response,
                    ::google::protobuf::Closure *done) override;
    /** 地图满员时入队等待。 */
    void EnqueueMap(::google::protobuf::RpcController *controller,
                    const ::sess::EnqueueMapRequest *request, ::sess::EnqueueMapResponse *response,
                    ::google::protobuf::Closure *done) override;
    /** 排空地图：拒新进并等待在场玩家离开。 */
    void DrainMap(::google::protobuf::RpcController *controller,
                  const ::sess::DrainMapRequest *request, ::sess::DrainMapResponse *response,
                  ::google::protobuf::Closure *done) override;
    /** 热迁移地图 Owner（在线迁移编排入口）。 */
    void LiveMigrateMap(::google::protobuf::RpcController *controller,
                        const ::sess::LiveMigrateMapRequest *request,
                        ::sess::LiveMigrateMapResponse *response,
                        ::google::protobuf::Closure *done) override;
    /** 读取权威 Placement 记录。 */
    void GetPlacement(::google::protobuf::RpcController *controller,
                      const ::sess::GetPlacementRequest *request,
                      ::sess::GetPlacementResponse *response,
                      ::google::protobuf::Closure *done) override;
    /** 冷迁移：换 Owner 并升 epoch（旧 epoch 写应被拒）。 */
    void MigrateMap(::google::protobuf::RpcController *controller,
                    const ::sess::MigrateMapRequest *request, ::sess::MigrateMapResponse *response,
                    ::google::protobuf::Closure *done) override;
    /** 标记 Placement 进入恢复中（故障转移）。 */
    void MarkRecovering(::google::protobuf::RpcController *controller,
                        const ::sess::MarkRecoveringRequest *request,
                        ::sess::MarkRecoveringResponse *response,
                        ::google::protobuf::Closure *done) override;
    /** Owner 续租：MapLeaseKeeper Tick 调用。 */
    void HeartbeatOwner(::google::protobuf::RpcController *controller,
                        const ::sess::HeartbeatOwnerRequest *request,
                        ::sess::HeartbeatOwnerResponse *response,
                        ::google::protobuf::Closure *done) override;
    /** 更新玩家路由（map_instance / Logic / route_version）。 */
    void UpdatePlayerRoute(::google::protobuf::RpcController *controller,
                           const ::sess::UpdatePlayerRouteRequest *request,
                           ::sess::UpdatePlayerRouteResponse *response,
                           ::google::protobuf::Closure *done) override;
    /** 玩家跨图/跨 Logic 转移三阶段：Begin。 */
    void BeginPlayerTransfer(::google::protobuf::RpcController *controller,
                             const ::sess::BeginPlayerTransferRequest *request,
                             ::sess::BeginPlayerTransferResponse *response,
                             ::google::protobuf::Closure *done) override;
    /** 玩家转移：Commit。 */
    void CommitPlayerTransfer(::google::protobuf::RpcController *controller,
                              const ::sess::CommitPlayerTransferRequest *request,
                              ::sess::CommitPlayerTransferResponse *response,
                              ::google::protobuf::Closure *done) override;
    /** 玩家转移：Abort。 */
    void AbortPlayerTransfer(::google::protobuf::RpcController *controller,
                             const ::sess::AbortPlayerTransferRequest *request,
                             ::sess::AbortPlayerTransferResponse *response,
                             ::google::protobuf::Closure *done) override;
    /** 查询玩家当前路由。 */
    void GetPlayerRoute(::google::protobuf::RpcController *controller,
                        const ::sess::GetPlayerRouteRequest *request,
                        ::sess::GetPlayerRouteResponse *response,
                        ::google::protobuf::Closure *done) override;

    /** 遗留 game.Login 入口（兼容）；正式路径走 Auth+AcquireSession。 */
    void Login(::google::protobuf::RpcController *controller, const ::game::LoginReq *request,
               ::game::LoginRsp *response, ::google::protobuf::Closure *done) override;
    void Reconnect(::google::protobuf::RpcController *controller,
                   const ::game::ReconnectReq *request, ::game::ReconnectRsp *response,
                   ::google::protobuf::Closure *done) override;
    void Logout(::google::protobuf::RpcController *controller, const ::game::LogoutReq *request,
                ::game::LogoutRsp *response, ::google::protobuf::Closure *done) override;
    /** 校验会话 token/fence（遗留 ValidateTokenReq）。 */
    void ValidateToken(::google::protobuf::RpcController *controller,
                       const ::sess::ValidateTokenReq *request, ::sess::ValidateTokenRsp *response,
                       ::google::protobuf::Closure *done) override;
    /** 绑定 TCP 连接到已有会话（遗留）。 */
    void BindConnection(::google::protobuf::RpcController *controller,
                        const ::sess::BindConnectionReq *request,
                        ::sess::BindConnectionRsp *response,
                        ::google::protobuf::Closure *done) override;
    /** 遗留 MarkDisconnected；新路径用 MarkDisconnectedV2。 */
    void MarkDisconnected(::google::protobuf::RpcController *controller,
                          const ::sess::MarkDisconnectedReq *request,
                          ::sess::MarkDisconnectedRsp *response,
                          ::google::protobuf::Closure *done) override;
};
