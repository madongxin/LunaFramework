#pragma once

#include <string>

class EventLoop;

struct RoleStartOpts {
    std::string role;
    int http_port = 0;
    int game_port = 0;
    int logic_port_override = -1;
};

/** Start brpc/role-specific services. Returns 0 on success. */
int BootstrapStartRoleServices(EventLoop *loop, const RoleStartOpts &opts);
