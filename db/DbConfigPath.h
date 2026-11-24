#pragma once

/**
 * @file DbConfigPath.h
 * @brief mysql.cnf 等 DB 配置路径（按 exe 解析项目 config/）
 */

#include "LunaServerPaths.h"

#include <string>

namespace DbConfigPath {

/** mysql.cnf 绝对或相对路径 */
inline const std::string &MysqlCnf() {
    static std::string path = "../config/mysql.cnf";
    static bool once = false;
    if (!once) {
        once = true;
        std::string resolved;
        if (LunaServerPaths::ResolveProjectSubdir("config/mysql.cnf", &resolved))
            path = std::move(resolved);
    }
    return path;
}

}  // namespace DbConfigPath
