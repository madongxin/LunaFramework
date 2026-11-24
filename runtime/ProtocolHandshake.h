#pragma once

/**
 * @file ProtocolHandshake.h
 * @brief ClientHello / ServerHello 协议握手与心跳/空闲超时参数
 *
 * HandleClientHello 纯内存校验（版本/schema），不访问 Redis/MySQL。
 * Formal 下 ClientHelloRequired 强制握手；见 FormalMode。
 */

#include "game.pb.h"

#include <cstdint>
#include <string>

namespace gameproto {

constexpr uint32_t kPublicProtocolVersion = 1;
constexpr uint32_t kMinSupportedProtocolVersion = 1;
constexpr uint32_t kDefaultHeartbeatIntervalMs = 15000;
constexpr uint32_t kDefaultIdleTimeoutMs = 45000;
constexpr uint32_t kDefaultHelloDeadlineMs = 10000;

/** 编译期嵌入的 schema SHA-256。 */
const char *CompiledSchemaSha256();
std::string EffectiveSchemaSha256();
/** 服务端构建标识，写入 ServerHello。 */
std::string ServerBuildId();
uint32_t HeartbeatIntervalMs();
/** 服务端空闲踢人超时。 */
uint32_t IdleTimeoutMs();
uint32_t HelloDeadlineMs();
uint32_t HeartbeatMinIntervalMs();

/** 客户端版本是否过旧需强制升级。 */
bool ClientVersionTooOld(const std::string &client_version);

/** 处理 ClientHello；结果写入 *hello。不访问 Redis/MySQL。 */
void HandleClientHello(const game::ClientHelloReq &req, uint64_t conn_id, game::ServerHelloRsp *hello,
                       game::GameResponse *outer);

}  // namespace gameproto
