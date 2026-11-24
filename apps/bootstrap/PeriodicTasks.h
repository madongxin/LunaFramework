#pragma once

#include <string>

class EventLoop;

/**
 * 按角色往主 EventLoop 上挂周期任务：
 * 健康续约、Session 宽限过期、地图租约、指标落库、注册表 Renew 等。
 * 在 BootstrapStartRoleServices 之后、进入 Loop 之前调用。
 */
void BootstrapInstallPeriodicTasks(EventLoop *loop, const std::string &role);
