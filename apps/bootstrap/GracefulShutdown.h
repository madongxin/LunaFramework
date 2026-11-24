#pragma once

#include <string>

class GameTcpGateway;

/**
 * 主循环退出后的优雅收尾（在 HttpServer::start()→TcpServer::Loop 返回之后调用）。
 *
 * 顺序概要：SetDraining / ready=false → 停 accept / 好友 fanout
 * → 注册表 DRAINING → 可选 drain 等待（LUNASERVER_DRAIN_SEC）
 * → Unregister → 停 brpc / AOI / FlushLastSafe → 停 Game TCP。
 */
void BootstrapGracefulShutdown(const std::string &role, GameTcpGateway *&game_tcp_gw);
