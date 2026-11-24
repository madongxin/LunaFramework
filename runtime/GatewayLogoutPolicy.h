#pragma once

/**
 * @file GatewayLogoutPolicy.h
 * @brief Logout 结果判定：编码成功 ≠ Session 离线 ≠ Logic/AOI 清理
 *
 * LogoutAuthoritativeOk 要求 session_ok && logic_ok；仅权威成功时清绑定。
 */

namespace gameproto {

/** Logout 三类结果：编码成功 ≠ Session 离线 ≠ Logic/AOI 清理成功。 */
struct GatewayLogoutResult {
    bool encoded = false;
    bool session_ok = false;
    bool logic_ok = false;
    bool clear_bind = false;
};

/** Session 与 Logic/AOI 均成功才算权威登出成功。 */
inline bool LogoutAuthoritativeOk(const GatewayLogoutResult &r) {
    return r.session_ok && r.logic_ok;
}

/** 仅权威成功时清除 Gateway 本地绑定。 */
inline bool LogoutShouldClearBind(const GatewayLogoutResult &r) {
    return LogoutAuthoritativeOk(r);
}

}  // namespace gameproto
