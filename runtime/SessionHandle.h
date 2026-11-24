#pragma once

/**
 * @file SessionHandle.h
 * @brief 业务侧会话句柄：player/连接/fence 与地图路由字段
 *
 * Gateway 绑定玩家后在投递与推送路径携带本结构。generation/fence_token 用于
 * 防重入与 Session fence 校验；map_instance_id / owner_epoch / route_version /
 * gamelogic_instance_id 支撑阶段 5 地图归属与跨 Logic 路由。
 *
 * 协作：ITransport::PostPlayerRequest、Push 路径、EnterMap 编排。
 */

#include <cstdint>
#include <string>

/** 业务侧会话句柄（阶段 3 fence + 阶段 5 地图路由）。 */
struct SessionHandle {
    uint64_t player_id = 0;
    uint64_t connection_id = 0;
    /** 会话代次；与 Redis Session generation 对齐，防旧连接写新会话。 */
    uint64_t generation = 0;
    std::string session_id;
    /** Session fence token；推送/写路径校验用。 */
    std::string fence_token;
    uint64_t map_instance_id = 0;
    uint64_t owner_epoch = 0;
    uint64_t route_version = 0;
    /** 当前地图所在 GameLogic 实例；PushBatch 定向用。 */
    std::string gamelogic_instance_id;
};
