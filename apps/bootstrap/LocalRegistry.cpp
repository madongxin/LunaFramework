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

void RenewAll() {
    std::lock_guard<std::mutex> lk(Mu());
    for (const auto &r : Entries()) {
        StaticServiceRegistry::Get().RenewInstance(r.service, r.instance_id, r.ttl_sec);
        if (RedisServiceRegistry::Get().ready())
            RedisServiceRegistry::Get().RenewInstance(r.service, r.instance_id, r.ttl_sec);
    }
}

std::string ResolveInstanceId(const std::string &prefix, int port, const char *fallback) {
    if (const char *env = std::getenv("LUNASERVER_INSTANCE_ID")) {
        if (env[0])
            return env;
    }
    if (fallback && fallback[0])
        return fallback;
    return prefix + "-" + std::to_string(port > 0 ? port : 0);
}

}  // namespace LocalRegistry
