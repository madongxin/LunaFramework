#pragma once

#include <cstdlib>
#include <cstring>
#include <string>

/** 管理口访问判定。正式模式且没有配置令牌时，只允许回环地址。 */
struct HttpAdminAccessDecision {
    bool allow = true;
    const char *reason = "open";
};

inline bool HttpPeerIsLoopback(const std::string &ip) {
    return ip == "127.0.0.1" || ip == "::1" || ip == "localhost";
}

/** 公网无令牌放开观察页：LUNASERVER_HTTP_MONITOR_OPEN=1（仅监控相关路径）。 */
inline bool HttpMonitorOpenFromEnv() {
    const char *v = std::getenv("LUNASERVER_HTTP_MONITOR_OPEN");
    return v && (std::strcmp(v, "1") == 0 || std::strcmp(v, "true") == 0 ||
                 std::strcmp(v, "TRUE") == 0 || std::strcmp(v, "on") == 0);
}

/** /monitor 及其前端依赖的只读 API；不含 /gm 等危险管理口。 */
inline bool IsHttpMonitorPublicPath(const std::string &url) {
    std::string path = url;
    const size_t q = path.find('?');
    if (q != std::string::npos)
        path.resize(q);
    if (path == "/monitor" || path == "/monitor.html")
        return true;
    // 观察页本地 Chart.js 等静态依赖（公网免令牌时也要能拉到）。
    if (path.compare(0, 8, "/vendor/") == 0)
        return true;
    if (path == "/api/cluster" || path == "/api/monitor/live" || path == "/api/version")
        return true;
    if (path.compare(0, 16, "/api/prometheus/") == 0)
        return true;
    if (path.compare(0, 13, "/api/metrics/") == 0)
        return true;
    return false;
}

inline std::string HttpQueryParam(const std::string &url, const std::string &key) {
    const size_t qpos = url.find('?');
    if (qpos == std::string::npos)
        return "";
    const std::string prefix = key + "=";
    size_t pos = qpos + 1;
    while (pos < url.size()) {
        const size_t amp = url.find('&', pos);
        const std::string part = url.substr(pos, amp == std::string::npos ? std::string::npos : amp - pos);
        if (part.compare(0, prefix.size(), prefix) == 0)
            return part.substr(prefix.size());
        if (amp == std::string::npos)
            break;
        pos = amp + 1;
    }
    return "";
}

inline std::string HttpCookieValue(const std::string &cookie_header, const std::string &key) {
    const std::string prefix = key + "=";
    size_t pos = 0;
    while (pos < cookie_header.size()) {
        while (pos < cookie_header.size() && (cookie_header[pos] == ' ' || cookie_header[pos] == ';'))
            ++pos;
        const size_t semi = cookie_header.find(';', pos);
        const std::string part =
            cookie_header.substr(pos, semi == std::string::npos ? std::string::npos : semi - pos);
        if (part.compare(0, prefix.size(), prefix) == 0)
            return part.substr(prefix.size());
        if (semi == std::string::npos)
            break;
        pos = semi + 1;
    }
    return "";
}

inline HttpAdminAccessDecision EvaluateHttpAdminAccess(bool formal, const std::string &peer_ip,
                                                       const std::string &configured_token,
                                                       const std::string &presented_token) {
    HttpAdminAccessDecision d;
    // 回环留给本机观察页和 Prometheus 采集，不要求令牌。
    if (HttpPeerIsLoopback(peer_ip)) {
        d.allow = true;
        d.reason = "loopback";
        return d;
    }
    if (!configured_token.empty()) {
        if (presented_token == configured_token) {
            d.allow = true;
            d.reason = "token";
            return d;
        }
        d.allow = false;
        d.reason = "bad token";
        return d;
    }
    if (formal) {
        d.allow = false;
        d.reason = "formal non-loopback requires token";
        return d;
    }
    d.allow = true;
    d.reason = "non-formal";
    return d;
}
