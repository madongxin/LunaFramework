#pragma once

#include <string>

/**
 * RunServer / 管理 HTTP 共享的进程状态。
 * 定义在 ServerBootstrap.cpp，供 AdminHttpRouter、监控页展示角色与 Game 端口。
 */
extern std::string g_process_role;  // gateway | gamelogic | session | gamedb | …
extern int g_game_port;            // 玩家 Game TCP；非 gateway 角色为 0
