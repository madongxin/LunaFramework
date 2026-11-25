#pragma once

#include <string>
#include <vector>

/**
 * 扁平 cnf（key=value）读取与地址列表解析。
 * 启动期把 gateway_push_addrs 等灌进 Push 客户端 / 静态服务注册表。
 */

/** 读 cnf 中第一个匹配的 key；找不到返回空串。 */
std::string LoadCnfValue(const std::string &path, const char *key);

/**
 * 按项目相对路径读 cnf（如 config/gamelogic.cnf）。
 * 先 ResolveProjectSubdir，失败再回退 ../rel_path。
 */
std::string LoadProjectCnfValue(const char *rel_path, const char *key);

/** 逗号分隔地址列表，去掉空白。 */
std::vector<std::string> SplitAddrList(const std::string &csv);

/** 从网关/逻辑相关 cnf 应用 gateway_push_addrs → GatewayPushClient + StaticServiceRegistry。 */
void ApplyGatewayPushAddrsFromConfig();
