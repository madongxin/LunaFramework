#pragma once

/**
 * @file MailConfigPath.h
 * @brief 解析 config/mail.cnf 的绝对/相对路径（相对可执行文件工作目录）
 *
 * 优先 LunaServerPaths::ResolveProjectSubdir；失败回退 ../config/mail.cnf。
 */

#include "LunaServerPaths.h"

#include <string>

namespace MailConfigPath {

/** 进程内缓存的 mail.cnf 路径（首次调用时解析一次）。 */
inline const std::string &MailCnf() {
    static std::string path = "../config/mail.cnf";
    static bool once = false;
    if (!once) {
        once = true;
        std::string resolved;
        if (LunaServerPaths::ResolveProjectSubdir("config/mail.cnf", &resolved))
            path = std::move(resolved);
    }
    return path;
}

}  // namespace MailConfigPath
