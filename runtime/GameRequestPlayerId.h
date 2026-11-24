#pragma once

/**
 * @file GameRequestPlayerId.h
 * @brief 从 GameRequest 载荷解析 player_id 的辅助
 */

#include <cstdint>
#include <string>

namespace gameproto {

/** 从 GameRequest 序列化体提取用于串行分片的 player_id；失败返回 0 */
uint64_t ExtractPlayerIdFromRequestPayload(const std::string &request_payload);

}  // namespace gameproto
