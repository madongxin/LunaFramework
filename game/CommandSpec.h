#pragma once

/**
 * @file CommandSpec.h
 * @brief GameRequest oneof 的命令规格表（路由、鉴权、危险标记）
 *
 * 新增 GameRequest 字段必须在表中补一行，否则 AuthenticatedClient 默认拒绝。
 * Gateway 用 global_service/mail 决定转发目标；GameLogic 用 needs_session_token
 * 决定是否 RequireSessionToken。与 CommandPolicy / TrustedPlayerId 测试锁定配合。
 */

#include "game.pb.h"

namespace gameproto {

/** 一条公网命令的路由和鉴权元数据。 */
struct CommandSpec {
    game::GameRequest::BodyCase body;
    bool global_service;       // Gateway 转到 GlobalService（含邮件）
    bool mail;                 // 邮件帧，Formal 下可转 GameDB
    bool preauth_allow;        // 未登录连接可以发
    bool authenticated_allow;  // 已登录连接可以发
    bool dangerous;            // 已登录也要 LUNASERVER_ALLOW_UNSAFE_DEBUG_COMMANDS
    bool needs_session_token;  // GameLogic::Handle 入口要校验 token
};

/** 按 body_case 查规格；未登记返回 nullptr（策略层应拒绝）。 */
const CommandSpec *FindCommandSpec(game::GameRequest::BodyCase body);
/** 返回静态表首址与元素个数（单测遍历完整性）。 */
const CommandSpec *CommandSpecTable(int *count);

}  // namespace gameproto
