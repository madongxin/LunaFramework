#pragma once

/**
 * @file CommandPolicy.h
 * @brief 公网 TCP 命令策略：按信任级别与 Formal 模式决定 Allow/Forbid
 *
 * 未登记 body 对 AuthenticatedClient 默认拒绝（须在 CommandSpec 表登记）。
 * Formal 下即使开启 LUNASERVER_ALLOW_UNSAFE_DEBUG_COMMANDS 也拒绝客户端危险命令；
 * Internal/Admin 信任级别可执行危险命令。Gateway 在拆帧后、转发前调用。
 */

#include "game.pb.h"

#include <string>

namespace gameproto {

/** 调用方身份：预登录客户端 / 已绑定客户端 / 内网服务 / 管理面。 */
enum class CommandTrustLevel {
    PreAuthClient = 0,
    AuthenticatedClient = 1,
    InternalService = 2,
    AdminService = 3,
};

enum class CommandDecision {
    Allow = 0,
    Forbid = 1,
};

/**
 * 是否允许危险调试命令（GrantItem/MailDeliver 等）。
 * Formal 恒为 false；非 Formal 需显式设 LUNASERVER_ALLOW_UNSAFE_DEBUG_COMMANDS=1。
 */
bool AllowUnsafeDebugCommands();

/**
 * 按 body + 信任级别 + formal_mode 判定是否放行。
 * 失败时 *err_code 一般为 ERR_COMMAND_FORBIDDEN 或更细错误码。
 */
CommandDecision ValidateCommandPolicy(game::GameRequest::BodyCase body, CommandTrustLevel trust,
                                      bool formal_mode, std::string *err_code = nullptr);

/**
 * 解析 payload 得到 body 后做公网 TCP 策略校验。
 * @param bound 连接是否已绑定玩家（决定 PreAuth vs Authenticated）
 * @return false 时 *err_code 填拒绝原因，调用方应回错误帧或关连
 */
bool AllowClientTcpPayload(const std::string &payload, bool bound, bool formal_mode,
                           std::string *err_code = nullptr);

}  // namespace gameproto
