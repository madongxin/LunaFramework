#pragma once

/**
 * @file BrpcConfigPath.h
 * @brief 解析 config/brpc.cnf 路径（与 MailConfigPath 同模式）
 */

#include "LunaServerPaths.h"

#include <string>

namespace BrpcConfigPath {

/** 进程内缓存的 brpc.cnf 路径（首次调用时解析一次）。 */
inline const std::string &Cnf() {
    static std::string path = "../config/brpc.cnf";
    static bool once = false;
    if (!once) {
        once = true;
        std::string resolved;
        if (LunaServerPaths::ResolveProjectSubdir("config/brpc.cnf", &resolved))
            path = std::move(resolved);
    }
    return path;
}

}  // namespace BrpcConfigPath
