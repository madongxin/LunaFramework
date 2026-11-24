/**
 * globalservice_main.cpp — GlobalService 进程入口（二进制 globalservice）。
 * 邮件等全局能力；经 brpc 对外，不直出玩家 TCP。
 * 路径: apps/globalservice_main.cpp
 */
#include "ServerBootstrap.h"

int main(int argc, char *argv[]) {
    LaunchOpts opts;
    opts.force_role = "globalservice";
    opts.log_basename = "globalservice";
    if (!ParseLaunchArgs(argc, argv, &opts))
        return 0;
    return RunServer(opts);
}
