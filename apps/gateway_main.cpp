/**
 * gateway_main.cpp — Gateway 进程入口。
 * 强制 role=gateway，解析 HTTP/Game 端口后进入 RunServer。
 * 对外玩家 TCP + 内网 Push；登录编排 Auth→Session→BindPlayer。
 * 路径: apps/gateway_main.cpp
 */
#include "ServerBootstrap.h"

int main(int argc, char *argv[]) {
    LaunchOpts opts;
    opts.force_role = "gateway";
    opts.log_basename = "gateway";
    if (!ParseLaunchArgs(argc, argv, &opts))
        return 0;
    return RunServer(opts);
}
