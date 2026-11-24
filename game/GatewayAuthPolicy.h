#pragma once

/**
 * @file GatewayAuthPolicy.h
 * @brief Gateway 预鉴权：哪些帧由本机编排、哪些允许未绑定发送
 *
 * Login/Logout/Reconnect/Register 由 Gateway 编排（Auth→Session→Bind），
 * 不得当普通业务帧转发给 GameLogic。
 * 未绑定连接仅允许白名单（含 ClientHello/Heartbeat）；其余应拒绝或关连。
 */

#include <string>

namespace gameproto {

/**
 * 是否为 Gateway 自有鉴权/会话编排载荷（Login/Logout/Reconnect/Register）。
 * Parse 失败返回 false。
 */
bool IsGatewayOwnedAuthPayload(const std::string &payload);

/**
 * 未绑定前允许的白名单：Login / Register / Reconnect / ClientHello / Heartbeat。
 * Parse 失败返回 false。
 */
bool IsPreAuthWhitelistPayload(const std::string &payload);

}  // namespace gameproto
