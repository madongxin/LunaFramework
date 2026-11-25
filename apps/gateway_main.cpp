/**
 * @file gateway_main.cpp
 * @brief Gateway 进程入口。
 *
 * 强制 role=gateway，解析 HTTP/Game 端口后进入 RunServer。
 * 对外玩家 TCP + 内网 Push；登录编排 Auth→Session→BindPlayer。
 */

#include "ServerBootstrap.h"

#include <cstdio>

int main(int argc, char *argv[]) {
    LaunchOpts opts;
    opts.force_role = "gateway";
    opts.log_basename = "gateway";
    if (!ParseLaunchArgs(argc, argv, &opts)) {
        std::fprintf(stderr, "usage: %s [http_port] [game_port]\n", argv[0] ? argv[0] : "gateway");
        return 1;
    }
    return RunServer(opts);
}
