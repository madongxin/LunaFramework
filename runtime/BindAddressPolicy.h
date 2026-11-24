#pragma once

/**
 * @file BindAddressPolicy.h
 * @brief 监听地址策略：Formal 下拒绝 0.0.0.0/公网，除非 allow_public_bind
 *
 * 正式模式默认只允许 loopback 与 RFC1918/链路本地内网地址。公网 VIP 或 0.0.0.0
 * 需配置 allow_public_bind=true 或环境变量 LUNASERVER_ALLOW_PUBLIC_BIND=1
 *（例如 gamedb-0 公网 monitor 的显式例外）。
 *
 * 协作：各 brpc/HTTP/TCP 启动路径在 bind 前调用 EvaluateListenBind；FormalMode 提供 formal 标志。
 */

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

/** 监听地址策略判定结果：ok=false 时 error 含可读拒绝原因。 */
struct ListenBindDecision {
    bool ok = true;
    std::string error;
};

/**
 * 从 "host:port" 或 "[ipv6]:port" 中取出 host 部分。
 * 无端口的纯 host 原样返回；空串输入返回空。
 */
inline std::string ListenHostOf(const std::string &addr) {
    if (addr.empty())
        return {};
    if (addr[0] == '[') {
        const auto end = addr.find(']');
        if (end != std::string::npos)
            return addr.substr(1, end - 1);
    }
    const auto first = addr.find(':');
    const auto last = addr.rfind(':');
    if (first != std::string::npos && first == last)
        return addr.substr(0, first);
    return addr;
}

/**
 * IPv4 是否为 loopback / 私网 / 链路本地。
 * 覆盖 127/8、10/8、192.168/16、172.16–31/12、169.254/16；解析失败返回 false。
 */
inline bool Ipv4LoopbackOrPrivate(const std::string &host) {
    unsigned a = 0, b = 0, c = 0, d = 0;
    if (std::sscanf(host.c_str(), "%u.%u.%u.%u", &a, &b, &c, &d) != 4)
        return false;
    if (a > 255 || b > 255 || c > 255 || d > 255)
        return false;
    if (a == 127 || a == 10)
        return true;
    if (a == 192 && b == 168)
        return true;
    if (a == 172 && b >= 16 && b <= 31)
        return true;
    if (a == 169 && b == 254)
        return true;
    return false;
}

/**
 * 判定 listen_addr 在当前模式下是否允许绑定。
 * @param formal 正式模式；false 或 allow_public_bind=true 时直接放行
 * @param allow_public_bind 显式允许公网/0.0.0.0（配置或环境变量）
 * localhost、::1、Ipv4LoopbackOrPrivate 在 Formal 下仍允许；其它地址拒绝并写 error。
 */
inline ListenBindDecision EvaluateListenBind(const std::string &listen_addr, bool formal,
                                            bool allow_public_bind) {
    ListenBindDecision out;
    const std::string host = ListenHostOf(listen_addr);
    if (host.empty()) {
        out.ok = false;
        out.error = "listen address is empty";
        return out;
    }
    if (!formal || allow_public_bind)
        return out;
    if (host == "localhost" || host == "::1" || Ipv4LoopbackOrPrivate(host))
        return out;
    out.ok = false;
    out.error = "formal mode refuses listen on " + listen_addr +
                " without allow_public_bind=true";
    return out;
}

/**
 * 环境变量是否开启公网绑定例外。
 * LUNASERVER_ALLOW_PUBLIC_BIND=1/true/TRUE 时返回 true。
 */
inline bool AllowPublicBindFromEnv() {
    const char *v = std::getenv("LUNASERVER_ALLOW_PUBLIC_BIND");
    if (!v || !*v)
        return false;
    return std::strcmp(v, "1") == 0 || std::strcmp(v, "true") == 0 || std::strcmp(v, "TRUE") == 0;
}

/**
 * 配置字符串是否表示真值。
 * 接受 1/true/TRUE/yes；用于解析 allow_public_bind 等布尔配置项。
 */
inline bool ConfigFlagTrue(const std::string &val) {
    return val == "1" || val == "true" || val == "TRUE" || val == "yes";
}
