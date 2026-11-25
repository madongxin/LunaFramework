/**
 * 本进程已 Register 的服务实例列表，供 DRAINING / Unregister / Renew 批量操作。
 * 与 StaticServiceRegistry、RedisServiceRegistry 双写；etcd 路径由 StartRoles 单独处理。
 */
#include "bootstrap/LocalRegistry.h"

#include "IServiceRegistry.h"
#include "RedisServiceRegistry.h"

#include <mutex>
#include <vector>

namespace LocalRegistry {
namespace {

struct Entry {
    std::string service;
    std::string instance_id;
    int ttl_sec = 30;
};

std::mutex &Mu() {
    static std::mutex mu;
    return mu;
}

std::vector<Entry> &Entries() {
    static std::vector<Entry> v;
    return v;
}

}  // namespace

void Track(const std::string &service, const std::string &instance_id, int ttl_sec) {
    std::lock_guard<std::mutex> lk(Mu());
    // 同 service+id 只保留一条，避免 Renew/Unregister 重复打注册表
    for (auto &e : Entries()) {
        if (e.service == service && e.instance_id == instance_id) {
            e.ttl_sec = ttl_sec > 0 ? ttl_sec : e.ttl_sec;
            return;
        }
    }
    Entries().push_back({service, instance_id, ttl_sec});
}

void MarkDraining() {
    std::lock_guard<std::mutex> lk(Mu());
    for (const auto &r : Entries()) {
        StaticServiceRegistry::Get().SetInstanceStatus(r.service, r.instance_id, "DRAINING");
        if (RedisServiceRegistry::Get().ready())
            RedisServiceRegistry::Get().SetInstanceStatus(r.service, r.instance_id, "DRAINING");
    }
}

void UnregisterAll() {
    std::lock_guard<std::mutex> lk(Mu());
    for (const auto &r : Entries()) {
        StaticServiceRegistry::Get().UnregisterInstance(r.service, r.instance_id);
        if (RedisServiceRegistry::Get().ready())
            RedisServiceRegistry::Get().UnregisterInstance(r.service, r.instance_id);
    }
    Entries().clear();
}

/** 周期任务调用：续约 TTL，避免被发现层摘除。 */
void RenewAll() {
    std::lock_guard<std::mutex> lk(Mu());
    for (const auto &r : Entries()) {
        StaticServiceRegistry::Get().RenewInstance(r.service, r.instance_id, r.ttl_sec);
        if (RedisServiceRegistry::Get().ready())
            RedisServiceRegistry::Get().RenewInstance(r.service, r.instance_id, r.ttl_sec);
    }
}

std::string ResolveInstanceId(const std::string &prefix, int port, const char *fallback) {
    // 正式多开必须设 LUNASERVER_INSTANCE_ID（如 gw-0、gl-1），否则 Push/路由会对不上
    if (const char *env = std::getenv("LUNASERVER_INSTANCE_ID")) {
        if (env[0])
            return env;
    }
    if (fallback && fallback[0])
        return fallback;
    return prefix + "-" + std::to_string(port > 0 ? port : 0);
}

}  // namespace LocalRegistry
