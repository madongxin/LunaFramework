#pragma once

/**
 * @file PlacementAuthority.h
 * @brief Formal 地图写：权威 Placement 字段校验（与 Redis/RPC 拉取解耦）
 *
 * GameLogic 在进图写/Move 等路径用 ValidateAuthorityWrite 核对本地缓存的
 * PlacementRecord 是否仍 Ready、本 Logic 为 Owner、epoch/route/lease 未过期。
 * 纯函数便于单测；不访问 Redis。
 */

#include "PlacementStore.h"

#include <chrono>
#include <cstdint>
#include <string>

/** 当前 unix 秒（权威 lease_until 比较用）。 */
inline int64_t PlacementNowUnixSec() {
    using namespace std::chrono;
    return duration_cast<seconds>(system_clock::now().time_since_epoch()).count();
}

/**
 * 校验一次权威地图写是否允许。
 * @param require_complete_fence Formal：epoch/route 不得为 0，且必须精确匹配权威 epoch；
 *        非 Formal 兼容：epoch/route 为 0 时跳过对应比较。
 * @param err_code 失败时写入 ERR_PLACEMENT_NOT_READY / ERR_WRONG_GAMELOGIC_OWNER /
 *        ERR_STALE_EPOCH / ERR_ROUTE_STALE / ERR_LEASE_* 等
 */
inline bool ValidateAuthorityWrite(const PlacementRecord &auth, uint64_t req_epoch,
                                   uint64_t req_route_ver, const std::string &local_logic_id,
                                   std::string *err_code, int64_t now_unix_sec = 0,
                                   bool require_complete_fence = false) {
    if (now_unix_sec <= 0)
        now_unix_sec = PlacementNowUnixSec();
    if (auth.state != PlacementState::Ready) {
        if (err_code)
            *err_code = "ERR_PLACEMENT_NOT_READY";
        return false;
    }
    if (auth.owner_logic_server_id != local_logic_id) {
        if (err_code)
            *err_code = "ERR_WRONG_GAMELOGIC_OWNER";
        return false;
    }
    if (require_complete_fence) {
        if (req_epoch == 0 || req_route_ver == 0) {
            if (err_code)
                *err_code = "ERR_PLACEMENT_FENCE_REQUIRED";
            return false;
        }
        if (req_epoch != auth.owner_epoch) {
            if (err_code)
                *err_code = "ERR_STALE_EPOCH";
            return false;
        }
        if (req_route_ver < auth.route_version) {
            if (err_code)
                *err_code = "ERR_ROUTE_STALE";
            return false;
        }
    } else {
        if (req_epoch != 0 && req_epoch != auth.owner_epoch) {
            if (err_code)
                *err_code = "ERR_STALE_EPOCH";
            return false;
        }
        if (req_route_ver != 0 && req_route_ver < auth.route_version) {
            if (err_code)
                *err_code = "ERR_ROUTE_STALE";
            return false;
        }
    }
    if (auth.lease_until <= 0) {
        if (err_code)
            *err_code = "ERR_LEASE_MISSING";
        return false;
    }
    if (auth.lease_until < now_unix_sec) {
        if (err_code)
            *err_code = "ERR_LEASE_EXPIRED";
        return false;
    }
    return true;
}
