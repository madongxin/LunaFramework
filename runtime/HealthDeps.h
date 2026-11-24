#pragma once

/**
 * @file HealthDeps.h
 * @brief 按进程角色检查就绪依赖（Redis / MySQL / brpc 下游）
 *
 * EvaluateHealthDeps 供 /ready 组装：ok=false 时 detail 含可读原因。
 * draining 由 ServiceHealth 另判；LUNASERVER_FORCE_NOT_READY=1 强制失败（演练用）。
 *
 * 协作：HealthProbeStore::Refresh、HTTP /ready 处理器。
 */

#include <string>

/** 依赖检查结果：ok + 人类可读 detail。 */
struct HealthDepsResult {
    bool ok = true;
    std::string detail = "ok";
};

/**
 * 按角色检查关键依赖（Redis / MySQL / brpc 客户端等）。
 * role 如 gateway/session/gamelogic/gamedb；未知角色尽量保守失败。
 */
HealthDepsResult EvaluateHealthDeps(const std::string &role);
