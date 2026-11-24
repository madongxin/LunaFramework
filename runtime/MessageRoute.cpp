/**
 * @file MessageRoute.cpp
 * @brief MessageRoute 实现：经 CommandSpec 标记分流 Global / Mail / Logic
 */

#include "MessageRoute.h"

#include "CommandSpec.h"

namespace gameproto {

/** MailStore 读写类：CommandSpec::mail。 */
bool IsMailBoundRequest(const game::GameRequest &req) {
    const CommandSpec *s = FindCommandSpec(req.body_case());
    return s && s->mail;
}

/** GlobalService 入口类：CommandSpec::global_service。 */
bool IsGlobalServiceRequest(const game::GameRequest &req) {
    const CommandSpec *s = FindCommandSpec(req.body_case());
    return s && s->global_service;
}

/** 非 Global 即走 GameLogic（含邮件经 Logic/World 再转的路径判定由上层组合）。 */
bool IsLogicBoundRequest(const game::GameRequest &req) {
    return !IsGlobalServiceRequest(req);
}

}  // namespace gameproto
