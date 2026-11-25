/**
 * @file gamelogic_main.cpp
 * @brief GameLogic 进程入口。
 *
 * 强制 role=gamelogic；第二个参数为 brpc listen 端口。
 * 玩法权威、AOI、地图租约；不接受密码/凭证。
 */

#include "ServerBootstrap.h"

#include <cstdio>

int main(int argc, char *argv[]) {
    LaunchOpts opts;
    opts.force_role = "gamelogic";
    opts.log_basename = "gamelogic";
    if (!ParseLaunchArgs(argc, argv, &opts)) {
        std::fprintf(stderr, "usage: %s [http_port] [brpc_port]\n",
                     argv[0] ? argv[0] : "gamelogic");
        return 1;
    }
    return RunServer(opts);
}
