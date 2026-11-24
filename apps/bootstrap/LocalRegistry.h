#pragma once

#include <string>

/**
 * 本进程已登记的服务实例（etcd/Redis 注册表条目的本地镜像）。
 * 用于优雅退出：MarkDraining → UnregisterAll；周期 RenewAll 续约。
 */
namespace LocalRegistry {

void Track(const std::string &service, const std::string &instance_id, int ttl_sec = 30);
void MarkDraining();
void UnregisterAll();
void RenewAll();

/** 生成或解析 instance_id（环境变量 / 前缀+端口 / fallback）。 */
std::string ResolveInstanceId(const std::string &prefix, int port, const char *fallback);

}  // namespace LocalRegistry
