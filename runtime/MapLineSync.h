#pragma once


/**
 * MapLineSync.h — 分线状态同步与 Push（主要由 GameLogic 侧跑）。
 * 路径: runtime/MapLineSync.h
 */
#include <cstdint>

/** 向同模板在线玩家推送分线快照（message_type=map.lines.v1）。 */
namespace MapLineSync {

void Install();
void NotifyTemplate(uint32_t realm_id, uint64_t map_template_id);

}  // namespace MapLineSync
