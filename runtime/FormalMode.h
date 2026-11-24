#pragma once

/**
 * @file FormalMode.h
 * @brief 正式模式与危险能力开关（全头文件内联）
 *
 * LUNASERVER_FORMAL=1 时整体 fail-closed：禁止 Auth 本地降级、禁止 Logic 直写账号、
 * 默认强制 ClientHello、限制可直连 MySQL 的角色。各危险能力另有独立环境变量，
 * Formal 下多数开关即使误开也会被忽略（见各自函数注释）。
 *
 * 协作：BindAddressPolicy / HttpAdminAccess / CommandPolicy / Gateway 握手路径读取本文件。
 */

#include <cstdlib>
#include <cstring>
#include <string>

/**
 * 是否处于正式模式。
 * 判定：环境变量 LUNASERVER_FORMAL 为 1/true/TRUE/on；未设置或其它值视为非正式。
 * 正式模式下各危险能力默认关闭或强制走受控路径。
 */
inline bool FormalModeEnabled() {
    const char *v = std::getenv("LUNASERVER_FORMAL");
    return v && (std::strcmp(v, "1") == 0 || std::strcmp(v, "true") == 0 ||
                 std::strcmp(v, "TRUE") == 0 || std::strcmp(v, "on") == 0);
}

/**
 * 实验能力显式开关：仅当指定环境变量为 1/true/TRUE/on 时开启（默认关闭）。
 * @param env_name 环境变量名；空指针或空串一律返回 false。
 */
inline bool ExperimentalFeatureEnabled(const char *env_name) {
    if (!env_name || !*env_name)
        return false;
    const char *v = std::getenv(env_name);
    return v && (std::strcmp(v, "1") == 0 || std::strcmp(v, "true") == 0 ||
                 std::strcmp(v, "TRUE") == 0 || std::strcmp(v, "on") == 0);
}

/**
 * 邮件会话故障放行（仅显式联调）。
 * Formal 时恒为 false；非 Formal 且 LUNASERVER_MAIL_DEV_SKIP_SESSION 开启时才放行。
 * 未设置时 Redis/Session 不可用必须拒绝玩家邮件。
 */
inline bool MailDevSkipSession() {
    if (FormalModeEnabled())
        return false;
    return ExperimentalFeatureEnabled("LUNASERVER_MAIL_DEV_SKIP_SESSION");
}

/**
 * 危险 HTTP 管理接口总开关。
 * 默认关闭；LUNASERVER_ENABLE_ADMIN=1/true 时开启。与 HttpAdminAccess 令牌/回环判定配合使用。
 */
inline bool AdminHttpEnabled() {
    const char *v = std::getenv("LUNASERVER_ENABLE_ADMIN");
    return v && (std::strcmp(v, "1") == 0 || std::strcmp(v, "true") == 0);
}

/**
 * 联调危险公网命令（GrantItem/MailDeliver）是否允许。
 * 仅非 Formal 且 LUNASERVER_ALLOW_UNSAFE_DEBUG_COMMANDS 显式开启时返回 true；
 * Formal 时即使环境变量误开也必须拒绝（见 CommandPolicy）。
 */
inline bool AllowUnsafeDebugCommandsEnv() {
    if (FormalModeEnabled())
        return false;
    const char *v = std::getenv("LUNASERVER_ALLOW_UNSAFE_DEBUG_COMMANDS");
    return v && (std::strcmp(v, "1") == 0 || std::strcmp(v, "true") == 0 ||
                 std::strcmp(v, "TRUE") == 0 || std::strcmp(v, "on") == 0);
}

/**
 * 正式模式下是否允许本进程初始化 MySQL 连接池。
 * 仅 gamedb（及紧急 role=all）可直连；gateway/session/gamelogic/globalservice 必须经 GameDB brpc。
 * 非 Formal 时恒允许（便于本地单进程联调）。
 */
inline bool FormalModeAllowsMysql(const std::string &role) {
    if (!FormalModeEnabled())
        return true;
    return role == "gamedb" || role == "all";
}

/**
 * 运行期隐式 DDL（CREATE TABLE）是否允许。
 * 默认关闭；仅本地显式 LUNASERVER_BOOTSTRAP_DDL=1。Formal 同样必须显式开启，不会因正式模式自动放行。
 */
inline bool BootstrapDdlEnabled() {
    return ExperimentalFeatureEnabled("LUNASERVER_BOOTSTRAP_DDL");
}

/**
 * 是否强制要求 ClientHello。
 * Formal 默认强制；LUNASERVER_ALLOW_LEGACY_NO_HELLO=1 为有截止日的兼容开关
 *（文档约定 2026-09-15 后删除），开启后即使 Formal 也不强制 Hello。
 */
inline bool ClientHelloRequired() {
    if (ExperimentalFeatureEnabled("LUNASERVER_ALLOW_LEGACY_NO_HELLO"))
        return false;
    return FormalModeEnabled();
}
