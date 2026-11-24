/**
 * @file gamedb_main.cpp
 * @brief GameDB 进程入口。
 *
 * 强制 role=gamedb；Formal 下唯一直连 MySQL 的业务角色。
 * 默认可对公网开 HTTP 观察页（MONITOR_OPEN）。
 */

#include "ServerBootstrap.h"

int main(int argc, char *argv[]) {
    LaunchOpts opts;
    opts.force_role = "gamedb";
    opts.log_basename = "gamedb";
    if (!ParseLaunchArgs(argc, argv, &opts))
        return 0;
    return RunServer(opts);
}
