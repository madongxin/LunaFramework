#pragma once

/**
 * @file RedisConfigPath.h
 * @brief redis.cnf 路径解析
 */

#include "LunaServerPaths.h"

#include <string>

namespace RedisConfigPath {

/** redis.cnf 路径 */
inline const std::string &RedisCnf() {
    static std::string path = "../config/redis.cnf";
    static bool once = false;
    if (!once) {
        once = true;
        std::string resolved;
        if (LunaServerPaths::ResolveProjectSubdir("config/redis.cnf", &resolved))
            path = std::move(resolved);
    }
    return path;
}

}  // namespace RedisConfigPath
