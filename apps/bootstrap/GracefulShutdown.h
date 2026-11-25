#pragma once

#include <string>

class GameTcpGateway;

/**
 * 主循环退出后的优雅收尾（在 EventLoop::Loop 因信号 Quit 返回之后调用）。
 * 启动失败半途退出时也可调用：传 drain_sec_override=0 跳过等待。
 *
 * 顺序概要：SetDraining / ready=false → 停 accept / 好友 fanout
 * → 注册表 DRAINING → 可选 drain 等待
 * → Unregister → 停 brpc / AOI / FlushLastSafe → 停 Game TCP。
 *
 * @param drain_sec_override <0 用 LUNASERVER_DRAIN_SEC（默认 2，上限 60）；
 *        >=0 强制该秒数（启动失败传 0）。
 */
void BootstrapGracefulShutdown(const std::string &role, GameTcpGateway *&game_tcp_gw,
                               int drain_sec_override = -1);
