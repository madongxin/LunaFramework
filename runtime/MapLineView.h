#pragma once

/**
 * @file MapLineView.h
 * @brief 分线列表视图：进图/切线回包、QueryMapLines、Prometheus
 *
 * 从 PlacementStore（Redis 权威占用）组装分线信息；protobuf 填充仅在
 * WEBSERVER_ENABLE_GAME_PROTOBUF 下编译。PrometheusText 可按 owner 过滤本 Logic 实例。
 *
 * 协作：MapLineSync 推送、Gateway QueryMapLines、运维监控。
 */

#include "PlacementStore.h"

#include <cstdint>
#include <string>
#include <vector>

#ifdef WEBSERVER_ENABLE_GAME_PROTOBUF
#include "game.pb.h"
#endif

/** 分线列表：进图/切线回包、QueryMapLines、Prometheus。 */
namespace MapLineView {

/** realm_id==0 时回落默认 realm，避免空服查询打到错误键。 */
uint32_t EffectiveRealm(uint32_t realm_id);

#ifdef WEBSERVER_ENABLE_GAME_PROTOBUF
/** 单条分线结构体 → protobuf。 */
void CopyLine(const MapLineInfo &src, game::MapLineInfo *dst);
/** 填充某模板下全部分线；失败返回 false（存储不可用等）。 */
bool FillLines(uint32_t realm_id, uint64_t map_template_id,
               google::protobuf::RepeatedPtrField<game::MapLineInfo> *out);
/** 组装完整 QueryMapLinesRsp。 */
bool FillQueryRsp(uint32_t realm_id, uint64_t map_template_id, game::QueryMapLinesRsp *out);
#endif

/**
 * Redis 权威分线/副本占用导出为 Prometheus 文本。
 * owner_only 非空时只导出该 GameLogic 的实例。
 */
std::string PrometheusText(const std::string &owner_only);

}  // namespace MapLineView
