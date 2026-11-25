/**
 * @file session_main.cpp
 * @brief Session（含同进程 AuthService）入口。
 *
 * 强制 role=session；Auth 发 token，Session 管 fence/ONLINE；Auth 不创建 Session。
 */

#include "ServerBootstrap.h"

#include <cstdio>

int main(int argc, char *argv[]) {
    LaunchOpts opts;
    opts.force_role = "session";
    opts.log_basename = "session";
    if (!ParseLaunchArgs(argc, argv, &opts)) {
        std::fprintf(stderr, "usage: %s [http_port] [brpc_port]\n", argv[0] ? argv[0] : "session");
        return 1;
    }
    return RunServer(opts);
}
