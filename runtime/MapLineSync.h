#pragma once

/**
 * @file MapLineSync.h
 * @brief 分线状态变更后向同模板在线玩家推送快照
 *
 * Install 注册推送钩子；NotifyTemplate 在分线创建/关闭/人数变化后触发，
 * 载荷 message_type=map.lines.v1。主要由 GameLogic 侧跑。
 *
 * 协作：MapLineView 提供列表内容；Push 路径按 gateway_instance_id 定向。
 */

#include <cstdint>

/** 向同模板在线玩家推送分线快照（message_type=map.lines.v1）。 */
namespace MapLineSync {

/** 安装推送回调（进程启动时一次）。 */
void Install();
/** 通知某 realm+模板的分线列表已变，触发推送。 */
void NotifyTemplate(uint32_t realm_id, uint64_t map_template_id);

}  // namespace MapLineSync
