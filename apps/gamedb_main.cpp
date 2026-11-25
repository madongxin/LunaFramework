/**
 * @file gamedb_main.cpp
 * @brief GameDB 进程入口。
 *
 * 强制 role=gamedb；Formal 下唯一直连 MySQL 的业务角色。
 * 公网观察页由部署侧设 LUNASERVER_HTTP_BIND / MONITOR_OPEN / ALLOW_PUBLIC_BIND。
 */

#include "ServerBootstrap.h"

#include <cstdio>

int main(int argc, char *argv[]) {
    LaunchOpts opts;
    opts.force_role = "gamedb";
    opts.log_basename = "gamedb";
    if (!ParseLaunchArgs(argc, argv, &opts)) {
        std::fprintf(stderr, "usage: %s [http_port] [brpc_port]\n", argv[0] ? argv[0] : "gamedb");
        return 1;
    }
    return RunServer(opts);
}
