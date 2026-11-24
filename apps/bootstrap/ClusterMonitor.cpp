/**
 * 集群观察页数据源：读 inventory.tsv、本机探活 /live|/ready、拼 JSON、代理 PromQL。
 * Prom 查询有槽位上限，避免堵死 gamedb HTTP 线程。
 */
#include "bootstrap/ClusterMonitor.h"

#include "bootstrap/AdminHttpUtil.h"

#include "FormalMode.h"
#include "LunaServerPaths.h"
#include "HealthDeps.h"
#include "PrometheusClient.h"
#include "ServiceHealth.h"

#include <atomic>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <signal.h>
#include <sys/socket.h>
#include <unistd.h>


// ----- inventory /proc / 本机 HTTP 探活工具 -----
std::string InventoryTsvPath() {
    if (const char *run = std::getenv("LUNASERVER_RUN_DIR")) {
        if (*run)
            return std::string(run) + "/inventory.tsv";
    }
    std::string formal;
    if (LunaServerPaths::ResolveProjectSubdir("run/formal", &formal))
        return formal + "/inventory.tsv";
    return "";
}

bool PidAlive(int pid) {
    if (pid <= 0)
        return false;
    return ::kill(pid, 0) == 0;
}

std::string ProcComm(int pid) {
    std::string s = ReadFile("/proc/" + std::to_string(pid) + "/comm");
    while (!s.empty() && (s.back() == '\n' || s.back() == '\r'))
        s.pop_back();
    return s;
}

int64_t ProcRssKb(int pid) {
    const std::string st = ReadFile("/proc/" + std::to_string(pid) + "/status");
    const auto pos = st.find("VmRSS:");
    if (pos == std::string::npos)
        return 0;
    return std::strtoll(st.c_str() + pos + 6, nullptr, 10);
}

int HttpGetLocal(int port, const char *path, int timeout_ms, std::string *body) {
    if (port <= 0 || !path)
        return 0;
    const int fd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0)
        return 0;
    timeval tv{};
    tv.tv_sec = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms % 1000) * 1000;
    ::setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    ::setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(static_cast<uint16_t>(port));
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    if (::connect(fd, reinterpret_cast<sockaddr *>(&addr), sizeof(addr)) != 0) {
        ::close(fd);
        return 0;
    }
    std::ostringstream req;
    req << "GET " << path << " HTTP/1.0\r\nHost: 127.0.0.1\r\nConnection: close\r\n\r\n";
    const std::string s = req.str();
    if (::send(fd, s.data(), s.size(), 0) < 0) {
        ::close(fd);
        return 0;
    }
    std::string raw;
    char buf[2048];
    while (true) {
        const ssize_t n = ::recv(fd, buf, sizeof(buf), 0);
        if (n <= 0)
            break;
        raw.append(buf, static_cast<size_t>(n));
        if (raw.size() > 65536)
            break;
    }
    ::close(fd);
    int status = 0;
    if (raw.size() >= 12 && raw.compare(0, 5, "HTTP/") == 0)
        status = std::atoi(raw.c_str() + 9);
    const auto hdr_end = raw.find("\r\n\r\n");
    if (body) {
        if (hdr_end != std::string::npos)
            *body = raw.substr(hdr_end + 4);
        else
            *body = raw;
    }
    return status;
}

int ParseHttpPort(const std::string &field) {
    if (field.empty() || field == "-")
        return 0;
    const auto colon = field.rfind(':');
    if (colon != std::string::npos)
        return std::atoi(field.c_str() + colon + 1);
    return std::atoi(field.c_str());
}

struct ClusterProc {
    std::string role, iid, rpc, comm, live_detail, ready_detail;
    int pid = 0;
    int http_port = 0;
    int game_port = 0;
    int64_t rss_kb = 0;
    int live_http = 0;
    int ready_http = 0;
    bool alive = false;
    bool live = false;
    bool ready = false;
    std::string git_sha;
};

std::string JsonStringField(const std::string &body, const char *key) {
    const std::string pat = std::string("\"") + key + "\":\"";
    const auto pos = body.find(pat);
    if (pos == std::string::npos)
        return "";
    const auto begin = pos + pat.size();
    const auto end = body.find('"', begin);
    if (end == std::string::npos)
        return "";
    return body.substr(begin, end - begin);
}

void ProbeClusterProc(ClusterProc *p) {
    if (!p || !p->alive)
        return;
    if (p->pid == ::getpid()) {
        p->git_sha = JsonStringField(ServiceHealth::Instance().VersionJson(), "git_sha");
        p->live = ServiceHealth::Instance().IsLive(30);
        const HealthDepsResult deps = EvaluateHealthDeps(ServiceHealth::Instance().service());
        p->ready = ServiceHealth::Instance().ready() && deps.ok;
        p->live_detail = "self";
        p->ready_detail = deps.detail;
        p->live_http = p->live ? 200 : 503;
        p->ready_http = p->ready ? 200 : 503;
        return;
    }
    if (p->http_port <= 0)
        return;
    std::string body;
    // ready 路径可能同步 Ping GameDB（约 800ms），超时过短会误报未就绪
    p->live_http = HttpGetLocal(p->http_port, "/health/live", 1500, &body);
    p->live = (p->live_http == 200);
    p->live_detail = body.size() > 240 ? body.substr(0, 240) : body;
    body.clear();
    p->ready_http = HttpGetLocal(p->http_port, "/health/ready", 1500, &body);
    p->ready = (p->ready_http == 200);
    p->ready_detail = body.size() > 240 ? body.substr(0, 240) : body;
    body.clear();
    if (HttpGetLocal(p->http_port, "/api/version", 800, &body) == 200)
        p->git_sha = JsonStringField(body, "git_sha");
}


// ----- /api/cluster：进程表 + alive/ready 汇总 -----
Json::Value BuildClusterStatusJson() {
    Json::Value out;
    out["ok"] = true;
    const std::string path = InventoryTsvPath();
    out["inventory"] = path;
    std::vector<ClusterProc> rows;
    std::ifstream in(path);
    std::string line;
    while (std::getline(in, line)) {
        if (line.empty() || line[0] == '#')
            continue;
        std::istringstream iss(line);
        std::string role, iid, pid_s, rpc, http, game;
        if (!(iss >> role >> iid >> pid_s >> rpc >> http >> game))
            continue;
        ClusterProc p;
        p.role = role;
        p.iid = iid;
        p.pid = std::atoi(pid_s.c_str());
        p.rpc = rpc;
        p.http_port = ParseHttpPort(http);
        p.game_port = ParseHttpPort(game);
        p.alive = PidAlive(p.pid);
        p.comm = p.alive ? ProcComm(p.pid) : "";
        p.rss_kb = p.alive ? ProcRssKb(p.pid) : 0;
        rows.push_back(std::move(p));
    }
    std::vector<std::thread> workers;
    workers.reserve(rows.size());
    for (auto &p : rows)
        workers.emplace_back([&p]() { ProbeClusterProc(&p); });
    for (auto &t : workers)
        t.join();
    Json::Value procs(Json::arrayValue);
    int alive_n = 0, live_n = 0, ready_n = 0;
    for (const auto &r : rows) {
        Json::Value p;
        p["role"] = r.role;
        p["instance_id"] = r.iid;
        p["pid"] = r.pid;
        p["alive"] = r.alive;
        p["rpc_addr"] = r.rpc;
        p["http_port"] = r.http_port;
        p["game_port"] = r.game_port;
        p["comm"] = r.comm;
        p["rss_kb"] = static_cast<Json::Int64>(r.rss_kb);
        p["live"] = r.live;
        p["ready"] = r.ready;
        p["live_http"] = r.live_http;
        p["ready_http"] = r.ready_http;
        p["live_detail"] = r.live_detail;
        p["ready_detail"] = r.ready_detail;
        p["git_sha"] = r.git_sha;
        if (r.alive)
            ++alive_n;
        if (r.live)
            ++live_n;
        if (r.ready)
            ++ready_n;
        procs.append(p);
    }
    const int n = static_cast<int>(rows.size());
    out["process_n"] = n;
    out["alive_n"] = alive_n;
    out["live_n"] = live_n;
    out["ready_n"] = ready_n;
    out["expected_n"] = 9;
    out["all_alive"] = (n > 0 && alive_n == n);
    out["all_ready"] = (n > 0 && ready_n == n);
    out["processes"] = procs;
    if (n == 0) {
        out["ok"] = false;
        out["error"] = "inventory.tsv empty or missing";
    }
    return out;
}

std::string ScrapeLocal(int port) {
    const int fd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0)
        return {};
    timeval tv{};
    tv.tv_sec = 0;
    tv.tv_usec = 400000;
    ::setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    ::setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(static_cast<uint16_t>(port));
    ::inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);
    if (::connect(fd, reinterpret_cast<sockaddr *>(&addr), sizeof(addr)) != 0) {
        ::close(fd);
        return {};
    }
    const char *req = "GET /metrics HTTP/1.1\r\nHost: 127.0.0.1\r\nConnection: close\r\n\r\n";
    if (::send(fd, req, std::strlen(req), 0) <= 0) {
        ::close(fd);
        return {};
    }
    std::string raw;
    char buf[8192];
    while (raw.size() < 2 * 1024 * 1024) {
        const ssize_t n = ::recv(fd, buf, sizeof(buf), 0);
        if (n <= 0)
            break;
        raw.append(buf, static_cast<size_t>(n));
    }
    ::close(fd);
    const size_t hdr = raw.find("\r\n\r\n");
    if (hdr == std::string::npos)
        return {};
    return raw.substr(hdr + 4);
}

double MetricMax(const std::string &text, const char *name, const char *need) {
    double best = 0;
    bool saw = false;
    size_t pos = 0;
    const std::string key(name);
    while (pos < text.size()) {
        const size_t nl = text.find('\n', pos);
        const std::string line = text.substr(pos, nl == std::string::npos ? std::string::npos : nl - pos);
        pos = nl == std::string::npos ? text.size() : nl + 1;
        if (line.compare(0, key.size(), key) != 0)
            continue;
        if (line.size() > key.size() && line[key.size()] != ' ' && line[key.size()] != '{')
            continue;
        if (need && line.find(need) == std::string::npos)
            continue;
        const size_t sp = line.rfind(' ');
        if (sp == std::string::npos)
            continue;
        const double v = std::strtod(line.c_str() + sp + 1, nullptr);
        if (!saw || v > best)
            best = v;
        saw = true;
    }
    return saw ? best : 0;
}

double MetricSum(const std::string &text, const char *name, const char *need) {
    double sum = 0;
    size_t pos = 0;
    const std::string key(name);
    while (pos < text.size()) {
        const size_t nl = text.find('\n', pos);
        const std::string line = text.substr(pos, nl == std::string::npos ? std::string::npos : nl - pos);
        pos = nl == std::string::npos ? text.size() : nl + 1;
        if (line.compare(0, key.size(), key) != 0)
            continue;
        if (line.size() > key.size() && line[key.size()] != ' ' && line[key.size()] != '{')
            continue;
        if (need && line.find(need) == std::string::npos)
            continue;
        const size_t sp = line.rfind(' ');
        if (sp == std::string::npos)
            continue;
        sum += std::strtod(line.c_str() + sp + 1, nullptr);
    }
    return sum;
}

int MetricCount(const std::string &text, const char *name) {
    int n = 0;
    size_t pos = 0;
    const std::string key(name);
    while (pos < text.size()) {
        const size_t nl = text.find('\n', pos);
        const std::string line = text.substr(pos, nl == std::string::npos ? std::string::npos : nl - pos);
        pos = nl == std::string::npos ? text.size() : nl + 1;
        if (line.compare(0, key.size(), key) != 0)
            continue;
        if (line.size() > key.size() && line[key.size()] != ' ' && line[key.size()] != '{')
            continue;
        ++n;
    }
    return n;
}


// ----- /api/monitor/live：网关在线、出口、Move 等摘要 -----
Json::Value BuildMonitorLiveJson() {
    Json::Value out;
    Json::Value gws(Json::arrayValue);
    double online = 0, hold = 0, silence = 0, p99 = 0, eagain = 0, retrans = 0;
    double rx = 0, tx = 0, sendq = 0, reply_ms = 0, outbox = 0;
    int gw_ok = 0;
    const int ports[] = {8080, 8082};
    const char *names[] = {"gw-0", "gw-1"};
    for (int i = 0; i < 2; ++i) {
        const std::string body = ScrapeLocal(ports[i]);
        Json::Value g;
        g["name"] = names[i];
        g["ok"] = !body.empty();
        if (!body.empty())
            ++gw_ok;
        const double on = MetricSum(body, "lunaserver_online_players", nullptr);
        const double h = MetricMax(body, "lunaserver_latest_move_hold_ms", nullptr);
        const double s = MetricMax(body, "lunaserver_aoi_move_entity_max_silence_ms", nullptr);
        const double p = MetricMax(body, "lunaserver_reply_latency_p99_ms", "kind=\"move\"");
        const double e = MetricSum(body, "lunaserver_tcp_eagain_total", nullptr);
        const double r = MetricMax(body, "lunaserver_host_tcp_retrans_segs_total", nullptr);
        g["online"] = on;
        g["hold_ms"] = h;
        g["silence_ms"] = s;
        g["move_p99_ms"] = p;
        g["eagain"] = e;
        g["retrans"] = r;
        online += on;
        rx += MetricSum(body, "lunaserver_gateway_rx_bytes_total", nullptr);
        tx += MetricSum(body, "lunaserver_gateway_tx_bytes_total", nullptr);
        sendq = std::max(sendq, MetricMax(body, "lunaserver_send_pending_max_bytes", nullptr));
        reply_ms = std::max(reply_ms, MetricMax(body, "lunaserver_pkt_stage_max_ms", "stage=\"reply_queue\""));
        outbox += MetricSum(body, "lunaserver_control_outbox_current_frames", nullptr);
        if (h > hold)
            hold = h;
        if (s > silence)
            silence = s;
        if (p > p99)
            p99 = p;
        eagain += e;
        if (r > retrans)
            retrans = r;
        gws.append(g);
    }
    std::string session = ScrapeLocal(8093);
    if (session.empty())
        session = ScrapeLocal(8096);
    out["gateways"] = gws;
    out["gateway_ok"] = gw_ok;
    out["online"] = online;
    out["hold_ms"] = hold;
    out["silence_ms"] = silence;
    out["move_p99_ms"] = p99;
    out["eagain"] = eagain;
    out["retrans"] = retrans;
    out["rx_bytes"] = rx;
    out["tx_bytes"] = tx;
    out["send_pending_bytes"] = sendq;
    out["reply_queue_ms"] = reply_ms;
    out["outbox_frames"] = outbox;
    out["session_online"] = MetricMax(session, "lunaserver_session_online_players", nullptr);
    out["line_count"] = MetricCount(session, "lunaserver_map_line_occupancy");
    out["line_players"] = MetricSum(session, "lunaserver_map_line_occupancy", nullptr);
    out["dungeon_count"] = MetricCount(session, "lunaserver_map_dungeon_occupancy");
    out["dungeon_players"] = MetricSum(session, "lunaserver_map_dungeon_occupancy", nullptr);
    return out;
}

// 监控页的 Prometheus 查询是同步的。同时最多 4 个，给 /monitor、/api/cluster、/api/monitor/live 留出线程。
struct PromQuerySlot {
    bool held = false;
    PromQuerySlot() {
        int cur = inflight().load(std::memory_order_relaxed);
        while (cur < 4) {
            if (inflight().compare_exchange_weak(cur, cur + 1, std::memory_order_acq_rel)) {
                held = true;
                return;
            }
        }
    }
    ~PromQuerySlot() {
        if (held)
            inflight().fetch_sub(1, std::memory_order_acq_rel);
    }
    static std::atomic<int> &inflight() {
        static std::atomic<int> n{0};
        return n;
    }
};


// ----- /api/prometheus/query(_range)：限流后转发 Prometheus -----
void HandlePrometheusInstant(const HttpRequest &request, HttpResponse *response, bool head_only) {
    PromQuerySlot slot;
    if (!slot.held) {
        SendPlain(response, "prometheus busy", HttpResponse::HttpStatusCode::k503ServiceUnavailable);
        return;
    }
    std::string q = UrlDecodeParam(UrlParam(request, "q"));
    if (q.empty())
        q = UrlDecodeParam(UrlParam(request, "query"));
    if (q.empty()) {
        SendPlain(response, "missing query param q", HttpResponse::HttpStatusCode::k400BadRequest);
        return;
    }
    const auto r = PrometheusInstantQuery(q);
    if (!r.error.empty()) {
        SendPlain(response, r.error, HttpResponse::HttpStatusCode::k500internalServerError);
        return;
    }
    response->SetStatusCode(static_cast<HttpResponse::HttpStatusCode>(r.http_status));
    response->SetStatusMessage("OK");
    response->SetContentType("application/json");
    response->SetContentLength(static_cast<int>(head_only ? 0 : r.body.size()));
    response->SetBody(head_only ? "" : r.body);
}

void HandlePrometheusRange(const HttpRequest &request, HttpResponse *response, bool head_only) {
    PromQuerySlot slot;
    if (!slot.held) {
        SendPlain(response, "prometheus busy", HttpResponse::HttpStatusCode::k503ServiceUnavailable);
        return;
    }
    std::string q = UrlDecodeParam(UrlParam(request, "q"));
    if (q.empty())
        q = UrlDecodeParam(UrlParam(request, "query"));
    const std::string start = UrlDecodeParam(UrlParam(request, "start"));
    const std::string end = UrlDecodeParam(UrlParam(request, "end"));
    const std::string step = UrlDecodeParam(UrlParam(request, "step"));
    if (q.empty() || start.empty() || end.empty() || step.empty()) {
        SendPlain(response, "need q,start,end,step", HttpResponse::HttpStatusCode::k400BadRequest);
        return;
    }
    const auto r = PrometheusQueryRange(q, start, end, step);
    if (!r.error.empty()) {
        SendPlain(response, r.error, HttpResponse::HttpStatusCode::k500internalServerError);
        return;
    }
    response->SetStatusCode(static_cast<HttpResponse::HttpStatusCode>(r.http_status));
    response->SetStatusMessage("OK");
    response->SetContentType("application/json");
    response->SetContentLength(static_cast<int>(head_only ? 0 : r.body.size()));
    response->SetBody(head_only ? "" : r.body);
}

#if 0  // 故意崩溃样例（未接入路由）；保留备查，避免 -Werror 阻断正式构建
void TestHandlePrometheusRange(const HttpRequest &request, HttpResponse *response, bool head_only) {
    std::string q = UrlDecodeParam(UrlParam(request, "q"));
    if (q.empty())
        q = UrlDecodeParam(UrlParam(request, "query"));
    const std::string start = UrlDecodeParam(UrlParam(request, "start"));
    const std::string end = UrlDecodeParam(UrlParam(request, "end"));
    const std::string step = UrlDecodeParam(UrlParam(request, "step"));
    if (q.empty() || start.empty() || end.empty() || step.empty()) {
        SendPlain(response, "need q,start,end,step", HttpResponse::HttpStatusCode::k400BadRequest);
        return;
    }
    const auto r = PrometheusQueryRange(q, start, end, step);
    if (!r.error.empty()) {
        SendPlain(response, r.error, HttpResponse::HttpStatusCode::k500internalServerError);
        return;
    }
    response->SetStatusCode(static_cast<HttpResponse::HttpStatusCode>(r.http_status));
    response->SetStatusMessage("OK");
    response->SetContentType("application/json");
    response->SetContentLength(static_cast<int>(head_only ? 0 : r.body.size()));
    response->SetBody(head_only ? "" : r.body);
    int *p = nullptr;
    *p = 1;
}

enum class DumpType {
    NullPointer = 1,
    DivideByZero,
    BufferOverflow,
    UseAfterFree,
    DoubleFree,
    OutOfBounds,
    Abort,
    AssertFail,
    StackOverflow
};

void RecursiveStackOverflow() {
    char buffer[1024];
    memset(buffer, 0, sizeof(buffer));

    RecursiveStackOverflow();
}

void TestCppDump(DumpType type) {
    std::cout << "TestCppDump type = " << static_cast<int>(type) << std::endl;

    switch (type) {
    case DumpType::NullPointer: {
        // 空指针解引用，常见宕机原因
        int* p = nullptr;
        *p = 123;
        break;
    }

    case DumpType::DivideByZero: {
        // 整数除 0
        volatile int a = 10;
        volatile int b = 0;
        volatile int c = a / b;
        std::cout << c << std::endl;
        break;
    }

    case DumpType::BufferOverflow: {
        // 栈缓冲区溢出
        char buffer[8];
        strcpy(buffer, "this string is too long");
        std::cout << buffer << std::endl;
        break;
    }

    case DumpType::UseAfterFree: {
        // 释放后继续使用
        int* p = new int(100);
        delete p;

        *p = 200;
        std::cout << *p << std::endl;
        break;
    }

    case DumpType::DoubleFree: {
        // 重复释放
        int* p = new int(123);
        delete p;
        delete p;
        break;
    }

    case DumpType::OutOfBounds: {
        // 数组越界访问
        int arr[3] = {1, 2, 3};
        arr[100] = 999;
        std::cout << arr[100] << std::endl;
        break;
    }

    case DumpType::Abort: {
        // 主动触发 abort，通常会产生 core dump
        std::abort();
        break;
    }

    case DumpType::AssertFail: {
        // 断言失败
        assert(false && "Test assert crash");
        break;
    }

    case DumpType::StackOverflow: {
        // 递归导致栈溢出
        RecursiveStackOverflow();
        break;
    }

    default:
        std::cout << "Unknown dump type" << std::endl;
        break;
    }
}
#endif  // 故意崩溃样例

