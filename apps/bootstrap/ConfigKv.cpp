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
    std::string push_map = GatewayConfigPath::ReadValue("gateway_push_addrs");
    auto try_cnf = [&](const char *rel) {
        if (!push_map.empty())
            return;
        std::string p = std::string("../") + rel;
        std::string resolved;
        if (LunaServerPaths::ResolveProjectSubdir(rel, &resolved))
            p = resolved;
        push_map = LoadCnfValue(p, "gateway_push_addrs");
    };
    try_cnf("config/gamelogic.cnf");
    try_cnf("config/session.cnf");
    try_cnf("config/globalservice.cnf");
    auto entries = SplitAddrList(push_map);
    for (const auto &e : entries) {
        const auto eq = e.find('=');
        if (eq == std::string::npos)
            continue;
        const std::string id = e.substr(0, eq);
        const std::string addr = e.substr(eq + 1);
        GatewayPushClient::Instance().SetGatewayPushAddr(id, addr);
        IServiceRegistry::ServiceInstance inst;
        inst.service = "gateway_push";
        inst.instance_id = id;
        inst.address = addr;
        inst.status = "UP";
        StaticServiceRegistry::Get().RegisterInstance(inst);
    }
}
