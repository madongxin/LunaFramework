#pragma once

/**
 * @file GatewayLoginRoute.h
 * @brief Login/Reconnect 编排成功后的路由与可靠 Push 补发载荷
 *
 * 不依赖 brpc 头，便于 Gateway 与单测共享。
 * 含权威 Logic/map/epoch/route_version，以及重连后按序补发的 pending_push_payloads
 *（已是可直接 Send 的 GameResponse body 或帧载荷，视调用约定）。
 */

#include <cstdint>
#include <string>
#include <vector>

namespace gameproto {

/** Gateway 登录/重连成功后的粘性路由与补发上下文。 */
struct GatewayLoginRoute {
    uint64_t player_id = 0;
    std::string session_id;
    std::string fence_token;
    uint64_t generation = 0;
    std::string gamelogic_instance_id;
    uint64_t map_instance_id = 0;
    uint64_t map_owner_epoch = 0;
    uint64_t route_version = 0;
    /** 可靠 Push 缺口过大时需先下发全量快照。 */
    bool need_full_snapshot = false;
    uint64_t last_server_seq = 0;
    /** 重连成功且绑定后按序补发的可靠 Push 载荷。 */
    std::vector<std::string> pending_push_payloads;
};

}  // namespace gameproto
