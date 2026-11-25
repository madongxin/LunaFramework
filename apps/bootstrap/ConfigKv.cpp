/**
 * 扁平 cnf 读取与 Push 地址注入。
 * gateway_push_addrs 形如：gw-0=127.0.0.1:8181,gw-1=127.0.0.1:8183
 */
#include "bootstrap/ConfigKv.h"

#include "LunaServerPaths.h"
#include "GatewayConfigPath.h"
#include "GatewayPushClient.h"
#include "IServiceRegistry.h"

#include <fstream>
#include <string>
#include <vector>

std::string LoadCnfValue(const std::string &path, const char *key) {
    std::ifstream in(path);
    if (!in || !key || !*key)
        return {};
    std::string line;
    while (std::getline(in, line)) {
        if (line.empty() || line[0] == '#')
            continue;
        const auto eq = line.find('=');
        if (eq == std::string::npos)
            continue;
        std::string k = line.substr(0, eq);
        while (!k.empty() && k.back() == ' ')
            k.pop_back();
        if (k == key) {
            std::string v = line.substr(eq + 1);
            while (!v.empty() && (v.back() == '\r' || v.back() == '\n' || v.back() == ' '))
                v.pop_back();
            size_t i = 0;
            while (i < v.size() && v[i] == ' ')
                ++i;
            return v.substr(i);
        }
    }
    return {};
}

std::string LoadProjectCnfValue(const char *rel_path, const char *key) {
    if (!rel_path || !*rel_path)
        return {};
    std::string path = std::string("../") + rel_path;
    std::string resolved;
    if (LunaServerPaths::ResolveProjectSubdir(rel_path, &resolved))
        path = std::move(resolved);
    return LoadCnfValue(path, key);
}

std::vector<std::string> SplitAddrList(const std::string &csv) {
    std::vector<std::string> out;
    std::string cur;
    for (char c : csv) {
        if (c == ',') {
            while (!cur.empty() && cur.back() == ' ')
                cur.pop_back();
            size_t i = 0;
            while (i < cur.size() && cur[i] == ' ')
                ++i;
            cur = cur.substr(i);
            if (!cur.empty())
                out.push_back(cur);
            cur.clear();
        } else
            cur.push_back(c);
    }
    while (!cur.empty() && (cur.back() == ' ' || cur.back() == '\r'))
        cur.pop_back();
    size_t i = 0;
    while (i < cur.size() && cur[i] == ' ')
        ++i;
    cur = cur.substr(i);
    if (!cur.empty())
        out.push_back(cur);
    return out;
}

void ApplyGatewayPushAddrsFromConfig() {
    // 优先 GatewayConfigPath（已解析的运行配置），否则依次试各角色 cnf
    std::string push_map = GatewayConfigPath::ReadValue("gateway_push_addrs");
    if (push_map.empty())
        push_map = LoadProjectCnfValue("config/gamelogic.cnf", "gateway_push_addrs");
    if (push_map.empty())
        push_map = LoadProjectCnfValue("config/session.cnf", "gateway_push_addrs");
    if (push_map.empty())
        push_map = LoadProjectCnfValue("config/globalservice.cnf", "gateway_push_addrs");
    auto entries = SplitAddrList(push_map);
    for (const auto &e : entries) {
        const auto eq = e.find('=');
        if (eq == std::string::npos)
            continue;
        const std::string id = e.substr(0, eq);    // gateway_instance_id
        const std::string addr = e.substr(eq + 1); // host:push_port
        GatewayPushClient::Instance().SetGatewayPushAddr(id, addr);
        IServiceRegistry::ServiceInstance inst;
        inst.service = "gateway_push";
        inst.instance_id = id;
        inst.address = addr;
        inst.status = "UP";
        StaticServiceRegistry::Get().RegisterInstance(inst);
    }
}
