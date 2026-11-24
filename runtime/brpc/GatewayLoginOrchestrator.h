#pragma once


/**
 * @file GatewayLoginOrchestrator.h
 * @brief Gateway 登录编排：Auth.Login → Session.AcquireSession → GameLogic.BindPlayer
 *
 * 与 GatewayAuthFlow 代次、LoginAdmission 预算配合；密码不进入 GameLogic。失败须 End 编排并回客户端公开错误。
 */
#include "GatewayAuthPolicy.h"
#include "GatewayLoginRoute.h"
#include "GatewayLogoutPolicy.h"

#include <cstdint>
#include <functional>
#include <string>

namespace gameproto {

/**
 * 同步登录编排：Auth.Login → Session.AcquireSession → GameLogic.BindPlayer。
 * Auth 失败不创建 Session；Bind 失败回滚 Session。会阻塞调用线程；
 * 生产路径请用 BeginOrchestrateGatewayLogin（RpcOffloadPool + 回投队列）。
 */
bool OrchestrateGatewayLogin(const std::string &gateway_instance_id, uint64_t connection_id,
                             const std::string &request_payload, std::string *response_frame,
                             GatewayLoginRoute *route_out);

using GatewayAuthDone = std::function<void(bool ok, std::string response_frame, GatewayLoginRoute route)>;

/**
 * 异步登录：RPC 在 offload/bthread 执行，完成后经 done 回投。
 * 过载或无法占预算时返回 false；成功仅表示已受理。
 */
bool BeginOrchestrateGatewayLogin(const std::string &gateway_instance_id, uint64_t connection_id,
                                  const std::string &request_payload, uint64_t shard_key,
                                  GatewayAuthDone done);

/** 同步注册：Gateway → AuthService.Register → GameDB（不创建 Session）。 */
bool OrchestrateGatewayRegister(const std::string &request_payload, std::string *response_frame);
/** 异步注册；语义同 OrchestrateGatewayRegister。 */
bool BeginOrchestrateGatewayRegister(const std::string &request_payload, uint64_t shard_key,
                                     GatewayAuthDone done);

/** 同步重连：Session.ReconnectV2 → GameLogic.BindPlayer，写出完整路由。 */
bool OrchestrateGatewayReconnect(const std::string &gateway_instance_id, uint64_t connection_id,
                                 const std::string &request_payload, std::string *response_frame,
                                 GatewayLoginRoute *route_out);
/** 异步重连；过载返回 false。 */
bool BeginOrchestrateGatewayReconnect(const std::string &gateway_instance_id, uint64_t connection_id,
                                      const std::string &request_payload, uint64_t shard_key,
                                      GatewayAuthDone done);

/**
 * 登出编排：Session Logout + Logic/AOI 清理。
 * result 可选，供 GatewayLogoutPolicy 判定是否清本地绑定。
 */
bool OrchestrateGatewayLogout(const std::string &gateway_instance_id, uint64_t connection_id,
                              const std::string &request_payload, std::string *response_frame,
                              GatewayLogoutResult *result = nullptr);

/**
 * Bind 成功但连接已失效时的幂等 Session 补偿（MarkDisconnected，保留宽限）。
 * 不等于 Logout。
 */
void CompensateGatewaySession(uint64_t player_id, const std::string &session_id,
                              const std::string &fence_token, uint64_t generation);

}  // namespace gameproto
