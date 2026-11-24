#pragma once


/**
 * HealthDeps.h — 就绪依赖检查（Redis/MySQL/下游）。
 * 路径: runtime/HealthDeps.h
 */
#include <string>

struct HealthDepsResult {
    bool ok = true;
    std::string detail = "ok";
};

/**
 * 按角色检查关键依赖（Redis / MySQL / brpc 客户端等）。
 * draining 由调用方另判；LUNASERVER_FORCE_NOT_READY=1 强制失败（演练用）。
 */
HealthDepsResult EvaluateHealthDeps(const std::string &role);
