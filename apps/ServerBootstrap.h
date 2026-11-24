#pragma once

/**
 * @file ServerBootstrap.h
 * @brief 多角色进程启动：LaunchOpts、ParseLaunchArgs、RunServer
 *
 * gateway/gamelogic/session/gamedb/globalservice 各 main 填 force_role 后调 RunServer。
 */

#include <string>

/**
 * 进程启动参数。
 *
 * 两种入口都会用到：
 * - 多角色二进制（gateway_main / gamelogic_main / …）：先设 force_role，再 ParseLaunchArgs 只解析端口；
 * - 兼容入口 server：force_role 为空，argv[1] 可为 role 名。
 *
 * 端口语义因 role 而异（ParseLaunchArgs / RunServer 再细化）：
 * - gateway / all：http_port=管理 HTTP；game_port=玩家 Game TCP（默认 http+1）
 * - gamelogic / session / gamedb：第二个端口是 brpc listen（logic_port_override）
 * - globalservice：通常只有 HTTP（+ 内部 brpc 由配置决定）
 */
struct LaunchOpts {
    std::string role = "all";
    int http_port = 8080;
    int game_port = 0;              // gateway/all 玩家 TCP；0=默认 http+1
    int logic_port_override = -1;   // gamelogic/session/gamedb 的 brpc 端口覆盖；-1=未指定
    std::string log_basename = "server";
    /** 若非空，跳过 argv 里的 role 解析，强制该 role（多二进制入口用） */
    std::string force_role;
};

/**
 * 解析命令行到 opts。
 *
 * 约定（force_role 为空时）：
 *   argv[1] 可选 role：all|gateway|gamelogic|globalservice|session|gamedb
 *   其后：http_port [game_or_logic_port]
 *
 * force_role 非空时（如 gateway_main）：
 *   argv[1]=http_port，argv[2]=game_port（gateway）或 brpc 端口（logic/session/gamedb）
 *
 * 成功返回 true；opts 为空指针返回 false。
 */
bool ParseLaunchArgs(int argc, char *argv[], LaunchOpts *opts);

/**
 * 按 role 拉起整进程并阻塞到退出。
 *
 * 顺序概要：
 *   异步日志 → Formal 提示 → BootstrapPrepareRole
 *   → 主 EventLoop + SIGTERM/SIGINT 优雅退出
 *   → MySQL / Redis / RocksDB 初始化
 *   → HTTP 管理口（/metrics、/monitor、/live…）
 *   → BootstrapStartRoleServices（brpc 等）
 *   → 周期任务 →（gateway）Game TCP
 *   → SetReady → server.start()（内部 TcpServer::Loop 阻塞）
 *   → Loop 返回后 BootstrapGracefulShutdown 收尾
 *
 * 返回 0 表示正常退出；非 0 为启动失败。
 */
int RunServer(const LaunchOpts &opts);
