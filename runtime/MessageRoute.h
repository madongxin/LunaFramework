#pragma once

/**
 * @file MessageRoute.h
 * @brief 按 GameRequest 类型判定应路由到 GlobalService / GameDB 邮件 / GameLogic
 *
 * 依据 CommandSpec 上的 global_service / mail 等标记做分流，避免 Gateway/Logic
 * 硬编码 body_case 列表。IsLogicBoundRequest 当前定义为「非 GlobalService」。
 *
 * 协作：Gateway 转发与 GameLogic Forward；Formal 下邮件最终落 GameDB（MailStore）。
 */

#include "game.pb.h"

namespace gameproto {

/**
 * 是否应路由到 GlobalService（邮件编排入口 + 聊天/好友骨架等）。
 * 依据 CommandSpec::global_service。
 */
bool IsGlobalServiceRequest(const game::GameRequest &req);

/**
 * 是否为邮件读写类请求。
 * Formal 下 World/Global 再转 GameDB（MailStore 只在 GameDB 进程）。
 */
bool IsMailBoundRequest(const game::GameRequest &req);

/**
 * 场景/战斗/进图等仍走 GameLogic 的请求。
 * 当前实现：非 GlobalService 即视为 Logic-bound。
 */
bool IsLogicBoundRequest(const game::GameRequest &req);

}  // namespace gameproto
