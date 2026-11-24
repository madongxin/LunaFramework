#pragma once

#include <string>

class EventLoop;

/** BootstrapStartRoleServices 的端口/角色入参（来自 LaunchOpts）。 */
struct RoleStartOpts {
    std::string role;
    int http_port = 0;
    int game_port = 0;
    int logic_port_override = -1;  // gamelogic/session/gamedb brpc 端口；-1=配置默认
};

/**
 * 按 role 启动 brpc 服务端/客户端与角色特有接线。
 * 返回 0 成功；非 0 应让 RunServer 立刻退出。
 */
int BootstrapStartRoleServices(EventLoop *loop, const RoleStartOpts &opts);
