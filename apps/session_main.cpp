/**
 * session_main.cpp — Session（含同进程 AuthService）入口。
 * 强制 role=session；Auth 发 token，Session 管 fence/ONLINE；Auth 不创建 Session。
 * 路径: apps/session_main.cpp
 */
#include "ServerBootstrap.h"

int main(int argc, char *argv[]) {
    LaunchOpts opts;
    opts.force_role = "session";
    opts.log_basename = "session";
    if (!ParseLaunchArgs(argc, argv, &opts))
        return 0;
    return RunServer(opts);
}
