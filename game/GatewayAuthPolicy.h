#pragma once


/**
 * GatewayAuthPolicy.h — Gateway 预鉴权白名单与策略。
 * 路径: game/GatewayAuthPolicy.h
 */
#include <string>

namespace gameproto {

/** Login / Logout / Reconnect / Register 由 Gateway 编排 */
bool IsGatewayOwnedAuthPayload(const std::string &payload);

/** 未绑定前允许：Login / Register / Reconnect */
bool IsPreAuthWhitelistPayload(const std::string &payload);

}  // namespace gameproto
