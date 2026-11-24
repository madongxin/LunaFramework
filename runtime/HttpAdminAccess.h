#pragma once

/**
 * @file HttpAdminAccess.h
 * @brief HTTP 管理口访问判定：loopback / token / MONITOR_OPEN 路径白名单
 *
 * 判定顺序（EvaluateHttpAdminAccess）：回环放行 → 配置令牌校验 → Formal 非回环无令牌拒绝 →
 * 非正式模式放行。公网观察页另由 LUNASERVER_HTTP_MONITOR_OPEN 配合 IsHttpMonitorPublicPath 放开只读路径。
 *
 * 协作：AdminHttpEnabled（FormalMode）决定管理口是否启用；本文件只管「谁可以访问」。
 */

#include <cstdlib>
#include <cstring>
#include <string>

/** 管理口单次访问判定结果：allow + 可读 reason（便于日志/指标）。 */
struct HttpAdminAccessDecision {
    bool allow = true;
    const char *reason = "open";
};

/**
 * 对端 IP 是否视为本机回环。
 * 识别 127.0.0.1 / ::1 / localhost；回环访问不要求管理令牌。
 */
inline bool HttpPeerIsLoopback(const std::string &ip) {
    return ip == "127.0.0.1" || ip == "::1" || ip == "localhost";
}

/**
 * 是否允许公网无令牌访问观察页相关路径。
 * 环境变量 LUNASERVER_HTTP_MONITOR_OPEN=1/true/TRUE/on；仅配合 IsHttpMonitorPublicPath 使用。
 */
inline bool HttpMonitorOpenFromEnv() {
    const char *v = std::getenv("LUNASERVER_HTTP_MONITOR_OPEN");
    return v && (std::strcmp(v, "1") == 0 || std::strcmp(v, "true") == 0 ||
                 std::strcmp(v, "TRUE") == 0 || std::strcmp(v, "on") == 0);
}

/**
 * 路径是否属于监控观察页只读白名单。
 * 含 /monitor、/vendor/ 静态依赖、/api/cluster|monitor/live|version、prometheus/metrics 前缀；
 * 不含 /gm 等危险管理口。会剥离 query string 后再比较。
 */
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

/**
 * 从 URL query 中解析指定 key 的值（不做 URL decode）。
 * 找不到 key 或无 query 时返回空串。
 */
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

/**
 * 从 Cookie 头解析指定 key 的值（不做 URL decode）。
 * 支持分号分隔的多个 cookie；找不到时返回空串。
 */
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

/**
 * 综合判定管理口是否允许本次请求。
 * @param formal 是否正式模式（通常来自 FormalModeEnabled）
 * @param peer_ip 对端 IP；回环直接放行
 * @param configured_token 服务端配置的管理令牌；非空时必须与 presented_token 一致
 * @param presented_token 请求携带的令牌（query/header/cookie 由调用方解析后传入）
 * Formal 且非回环且无令牌配置时拒绝；非正式无令牌时放行（联调便利）。
 */
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
