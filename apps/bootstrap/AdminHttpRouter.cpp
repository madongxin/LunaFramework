/**
 * 管理口路径分发。鉴权在 HttpServer；此处按 URL 调 ClusterMonitor / GmAdmin / 健康检查等。
 *
 * 分区大致：
 *   /login /gm          — 需 ENABLE_ADMIN
 *   /metrics 与诊断     — Prometheus 文本、AOI/连接诊断
 *   /api/prometheus/*   — 监控页代理查询
 *   /health/* /api/*    — live/ready/version/cluster/monitor
 *   静态页 /monitor 等
 */
#include "bootstrap/AdminHttpRouter.h"

#include "bootstrap/AdminHttpUtil.h"
#include "bootstrap/BootstrapState.h"
#include "bootstrap/ClusterMonitor.h"
#include "bootstrap/GmAdmin.h"

#include "AoiEvidence.h"
#include "FormalMode.h"
#include "LunaServerPaths.h"
#include "HealthDeps.h"
#include "Logging.h"
#include "OpsMetrics.h"
#include "PrometheusMetrics.h"
#include "ResourceWatchdog.h"
#include "SafePath.h"
#include "ServiceHealth.h"

#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>

#ifdef WEBSERVER_ENABLE_MYSQL
#include "ConnectionPool.h"
#include "MetricsDbWriter.h"
#ifdef WEBSERVER_ENABLE_GAME_PROTOBUF
#include "MailService.h"
#include "MailTypes.h"
#endif
#endif
#ifdef WEBSERVER_ENABLE_GAME_PROTOBUF
#include "GatewayConnRegistry.h"
#endif
#ifdef WEBSERVER_ENABLE_REDIS
#include "PlacementStore.h"
#include "SessionStore.h"
#endif


// ---------------------------------------------------------------------------
// 统一回调：顺序匹配路径，先匹配先生效
// ---------------------------------------------------------------------------
void HttpResponseCallback(const HttpRequest &request, HttpResponse *response) {
    const bool head_only = (request.method() == HttpRequest::Method::kHead);
    const std::string &url = request.url();


    // ----- 危险管理：登录发 gm_sid Cookie；asan 账号可触发崩溃 -----
    if (url == "/login" && request.method() == HttpRequest::Method::kPost) {
        if (!AdminHttpEnabled()) {
            SendPlain(response, "admin disabled", HttpResponse::HttpStatusCode::k403Forbidden);
            return;
        }
        const std::string user = UrlParam(request, "username");
        const std::string pass = UrlParam(request, "password");
        if (user == "demo" && pass == "demo") {
            const std::string sid = NewGmSessionId();
            response->AddHeader("Set-Cookie", "gm_sid=" + sid + "; Path=/");
            SendHtml(response, "<html><body>login ok <a href=\"/gm\">GM</a></body></html>", head_only);
        } else if (user == "asan" && pass == "asan") {
            // ASan 触发接口默认关闭；需显式 LUNASERVER_ENABLE_ASAN_CRASH=1
            const char *asan = std::getenv("LUNASERVER_ENABLE_ASAN_CRASH");
            if (!(asan && std::strcmp(asan, "1") == 0)) {
                SendPlain(response, "asan crash disabled", HttpResponse::HttpStatusCode::k403Forbidden);
                return;
            }
            crash_uaf();
            SendPlain(response, "never");
        } else {
            SendPlain(response, "login failed", HttpResponse::HttpStatusCode::k403Forbidden);
        }
        return;
    }

    if (url == "/gm") {
        if (!AdminHttpEnabled()) {
            SendPlain(response, "admin disabled", HttpResponse::HttpStatusCode::k403Forbidden);
            return;
        }
        const std::string sid = GmSidFromRequest(request);
        if (!GmSessionValid(sid)) {
            Json::Value e;
            e["error"] = "gm_session_invalid";
            e["hint"] = "POST /login with username=demo password=demo";
            SendJson(response, e, HttpResponse::HttpStatusCode::k403Forbidden);
            return;
        }
        if (request.method() == HttpRequest::Method::kGet) {
            SendHtml(response, GmConsolePage(request, sid), head_only);
            return;
        }
        if (request.method() == HttpRequest::Method::kPost) {
            const std::string cmd = UrlParam(request, "cmd");
            SendJson(response, GmExecute(cmd));
            return;
        }
    }


    // ----- 本进程指标与诊断 -----
    if (url == "/metrics") {
        if (g_process_role == "gateway" || g_process_role == "all") {
#ifdef WEBSERVER_ENABLE_GAME_PROTOBUF
            OpsMetrics::Instance().SetOnlinePlayers(
                GatewayConnRegistry::Instance().BoundPlayerCount());
#endif
        }
        if (g_process_role == "session" || g_process_role == "all") {
#ifdef WEBSERVER_ENABLE_REDIS
            if (SessionStore::Instance().Available()) {
                const int64_t online = SessionStore::Instance().OnlinePlayerCount();
                OpsMetrics::Instance().SetSessionOnlinePlayers(online);
                if (PlacementStore::Instance().Available()) {
                    const int64_t occ = PlacementStore::Instance().SumAllOccupancy(64);
                    OpsMetrics::Instance().SetLineOccupancyTotal(occ);
                    OpsMetrics::Instance().SetGhostOccupancy(occ > online ? occ - online : 0);
                }
            }
#endif
        }
        SendPrometheusMetrics(response, BuildPrometheusMetricsText(), head_only);
        return;
    }
    if (url == "/aoi/visible") {
        SendPlain(response, AoiVisibleJson());
        return;
    }
    if (url == "/conn/stalled") {
        int n = std::atoi(UrlParam(request, "n").c_str());
        SendPlain(response, AoiStalledJson(n));
        return;
    }
    if (url == "/aoi/trace") {
        const uint64_t viewer = std::strtoull(UrlParam(request, "viewer").c_str(), nullptr, 10);
        const uint64_t subject = std::strtoull(UrlParam(request, "subject").c_str(), nullptr, 10);
        int limit = std::atoi(UrlParam(request, "n").c_str());
        SendPlain(response, AoiTraceJson(viewer, subject, limit));
        return;
    }

    // ----- 监控页：Prometheus 代理 + DB/GM API -----
    if (url == "/api/prometheus/query") {
        HandlePrometheusInstant(request, response, head_only);
        return;
    }
    if (url == "/api/prometheus/query_range") {
        HandlePrometheusRange(request, response, head_only);
        return;
    }
#ifdef WEBSERVER_ENABLE_MYSQL
    if (url == "/api/db/ping") {
        Json::Value j;
        auto *pool = ConnectionPool::getconnectionPool();
        j["ok"] = pool->isInitialized();
        j["dbname"] = pool->dbname();
        j["table"] = "lunaserver_metrics";
        j["flush_interval_sec"] = 10;
        if (pool->isInitialized()) {
            MetricsHistoryQuery q;
            if (MetricsDbWriter::Instance().QueryHistory(1, 1, &q)) {
                j["row_count"] = static_cast<Json::UInt64>(q.total_rows);
                if (q.last_ts_unix)
                    j["last_ts_unix"] = static_cast<Json::Int64>(q.last_ts_unix);
                if (!q.error.empty())
                    j["error"] = q.error;
            }
        }
        SendJson(response, j);
        return;
    }
    if (url == "/api/metrics/history") {
        int hours = 6;
        int limit = 500;
        const std::string hs = UrlParam(request, "hours");
        const std::string ls = UrlParam(request, "limit");
        if (!hs.empty())
            hours = std::atoi(hs.c_str());
        if (!ls.empty())
            limit = std::atoi(ls.c_str());
        MetricsHistoryQuery q;
        MetricsDbWriter::Instance().QueryHistory(hours, limit, &q);
        Json::Value j;
        j["ok"] = q.ok;
        j["mysql"] = q.mysql;
        j["table"] = "lunaserver_metrics";
        j["hours"] = hours;
        j["limit"] = limit;
        j["row_count"] = static_cast<Json::UInt64>(q.total_rows);
        j["last_ts_unix"] = static_cast<Json::Int64>(q.last_ts_unix);
        j["point_n"] = static_cast<Json::UInt>(q.points.size());
        if (!q.error.empty())
            j["error"] = q.error;
        if (!q.mysql) {
            j["hint"] = "Formal 下落库只在 GameDB 进程；请打开 GameDB HTTP（默认 8094）/monitor";
        }
        Json::Value arr(Json::arrayValue);
        for (const auto &p : q.points) {
            Json::Value e;
            e["t"] = static_cast<Json::Int64>(p.ts_unix);
            e["cpu_seconds_total"] = p.cpu_seconds_total;
            e["rss_bytes"] = static_cast<Json::Int64>(p.rss_bytes);
            e["rss_kb"] = static_cast<Json::Int64>(p.rss_bytes / 1024);
            e["vm_size_bytes"] = static_cast<Json::Int64>(p.vm_size_bytes);
            e["open_fds"] = p.open_fds;
            e["process_threads"] = p.process_threads;
            e["eventloop_tick_sec"] = p.eventloop_tick_sec;
            e["eventloop_tick_peak_sec"] = p.eventloop_tick_peak_sec;
            e["thread_states_json"] = p.thread_states_json;
            e["tcp_send_queue_bytes"] = static_cast<Json::UInt64>(p.tcp_send_queue_bytes);
            e["tcp_recv_queue_bytes"] = static_cast<Json::UInt64>(p.tcp_recv_queue_bytes);
            arr.append(e);
        }
        j["points"] = arr;
        SendJson(response, j);
        return;
    }
#ifdef WEBSERVER_ENABLE_GAME_PROTOBUF
    if (url == "/api/gm/mail/deliver" && request.method() == HttpRequest::Method::kPost) {
        const std::string sid = GmSidFromRequest(request);
        if (!GmSessionValid(sid)) {
            Json::Value e;
            e["ok"] = false;
            e["error_code"] = "PERMISSION_DENIED";
            e["message"] = "gm_session_invalid";
            SendJson(response, e, HttpResponse::HttpStatusCode::k403Forbidden);
            return;
        }
        Json::Value root;
        Json::CharReaderBuilder b;
        std::string errs;
        const std::string &body = request.body();
        std::unique_ptr<Json::CharReader> reader(b.newCharReader());
        Json::Value out;
        if (!reader->parse(body.data(), body.data() + body.size(), &root, &errs)) {
            out["ok"] = false;
            out["error_code"] = "INVALID_ARGUMENT";
            out["message"] = "invalid json: " + errs;
            SendJson(response, out, HttpResponse::HttpStatusCode::k400BadRequest);
            return;
        }
        mail::DeliverRequest d;
        d.source_system = root.get("source_system", "gm").asString();
        d.business_key = root.get("business_key", "").asString();
        d.receiver_type = root.get("receiver_type", "ROLE").asString();
        d.receiver_id = root.get("receiver_id", 0).asUInt64();
        d.template_id = root.get("template_id", "").asString();
        d.template_version = root.get("template_version", 1).asInt();
        d.category = root.get("category", "SYSTEM").asString();
        d.priority = root.get("priority", 0).asInt();
        d.sender_name = root.get("sender_name", "GM").asString();
        d.title = root.get("title", "").asString();
        d.body = root.get("body", "").asString();
        d.send_at = root.get("send_at", 0).asInt64();
        d.expire_at = root.get("expire_at", 0).asInt64();
        d.trace_id = root.get("trace_id", sid).asString();
        if (root.isMember("attachments") && root["attachments"].isArray()) {
            for (const auto &ja : root["attachments"]) {
                mail::DeliverAttachment a;
                a.asset_type = ja.get("asset_type", "ITEM").asString();
                a.asset_id = ja.get("asset_id", 0).asUInt64();
                a.count = ja.get("count", 0).asUInt();
                a.bind_type = ja.get("bind_type", "NONE").asString();
                a.payload = ja.get("payload", "").asString();
                d.attachments.push_back(a);
            }
        }
        uint64_t mail_id = 0;
        std::string ec, msg;
        const bool ok = MailService::Instance().Deliver(d, &mail_id, &ec, &msg);
        out["ok"] = ok;
        out["error_code"] = ec;
        out["message"] = msg;
        out["mail_id"] = static_cast<Json::UInt64>(mail_id);
        out["idempotent_hit"] = ok && msg.find("idempotent") != std::string::npos;
        LOG_INFO << "GM mail deliver actor_sid=" << sid.substr(0, 8)
                 << " receiver=" << d.receiver_id << " mail_id=" << mail_id << " ok=" << ok
                 << " code=" << ec;
        SendJson(response, out, ok ? HttpResponse::HttpStatusCode::k200K
                                   : HttpResponse::HttpStatusCode::k400BadRequest);
        return;
    }
#endif
#endif
    // /health/live|/api/liveness|/api/health：进程 EventLoop 心跳存活

    // ----- 存活/就绪/版本/集群/观察页 JSON -----
    if (url == "/health/live" || url == "/api/health" || url == "/api/liveness") {
        const bool live = ServiceHealth::Instance().IsLive(30);
        response->SetContentType("application/json");
        SendPlain(response, ServiceHealth::Instance().LivenessJson(),
                  live ? HttpResponse::HttpStatusCode::k200K
                       : HttpResponse::HttpStatusCode::k503ServiceUnavailable);
        return;
    }
    // /health/ready|/api/readiness：依赖 + 注册就绪；未 ready 不得进 LB
    if (url == "/health/ready" || url == "/api/readiness") {
        const HealthDepsResult deps = EvaluateHealthDeps(ServiceHealth::Instance().service());
        const bool ok = ServiceHealth::Instance().ready() && deps.ok;
        response->SetContentType("application/json");
        SendPlain(response, ServiceHealth::Instance().ReadinessJson(deps.ok, deps.detail),
                  ok ? HttpResponse::HttpStatusCode::k200K
                     : HttpResponse::HttpStatusCode::k503ServiceUnavailable);
        return;
    }
    if (url == "/api/version") {
        response->SetContentType("application/json");
        SendPlain(response, ServiceHealth::Instance().VersionJson());
        return;
    }
    if (url == "/api/cluster" || url == "/api/cluster/status") {
        SendJson(response, BuildClusterStatusJson());
        return;
    }
    if (url == "/api/monitor/live") {
        SendJson(response, BuildMonitorLiveJson());
        return;
    }
    if (url == "/api/files") {
        SendJson(response, BuildFilesJson());
        return;
    }
    if (url == "/api/stats") {
        Json::Value j;
        j["watchdog"] = ResourceWatchdog::Instance().LastSampleJson();
        const std::string hist = UrlParam(request, "history");
        if (!hist.empty())
            j["history"] = ResourceWatchdog::Instance().HistoryJson(std::atoi(hist.c_str()));
        SendJson(response, j);
        return;
    }
    if (url == "/api/game/status") {
        Json::Value j;
        j["enabled"] = g_game_port > 0;
        j["port"] = g_game_port;
        SendJson(response, j);
        return;
    }
    if (url == "/api/gateway") {
        const char *host = std::getenv("LUNASERVER_ADVERTISE_HOST");
        if (!host || !host[0])
            host = "127.0.0.1";
        int port = g_game_port;
        if (const char *ep = std::getenv("LUNASERVER_PUBLIC_GAME_PORT")) {
            const int parsed = std::atoi(ep);
            if (parsed > 0)
                port = parsed;
        }
        Json::Value j;
        j["gateway"] = std::string(host) + ":" + std::to_string(port);
        j["host"] = host;
        j["port"] = port;
        j["tls"] = false;
        SendJson(response, j);
        return;
    }
#ifdef WEBSERVER_ENABLE_REDIS
    if (url == "/api/redis/status") {
        Json::Value j;
        j["enabled"] = true;
        j["available"] = SessionStore::Instance().Available();
        SendJson(response, j);
        return;
    }
#endif
    if (url == "/api/echo") {
        Json::Value j;
        j["url"] = url;
        j["body"] = request.body();
        SendJson(response, j);
        return;
    }
    if (url == "/download") {
        if (!AdminHttpEnabled() || !GmSessionValid(GmSidFromRequest(request))) {
            SendPlain(response, "admin denied", HttpResponse::HttpStatusCode::k403Forbidden);
            return;
        }
        const std::string file = UrlParam(request, "file");
        std::string path;
        if (!file.empty() && SafeJoinUnderRoot(LunaServerPaths::FilesRoot(), file, &path))
            DownloadFile(file, response, head_only);
        else
            SendPlain(response, "missing file", HttpResponse::HttpStatusCode::k400BadRequest);
        return;
    }
    if (url == "/delete" && request.method() == HttpRequest::Method::kPost) {
        if (!AdminHttpEnabled() || !GmSessionValid(GmSidFromRequest(request))) {
            SendPlain(response, "admin denied", HttpResponse::HttpStatusCode::k403Forbidden);
            return;
        }
        const std::string file = UrlParam(request, "file");
        std::string path;
        if (!SafeJoinUnderRoot(LunaServerPaths::FilesRoot(), file, &path)) {
            SendPlain(response, "bad path", HttpResponse::HttpStatusCode::k400BadRequest);
            return;
        }
        const int rc = ::remove(path.c_str());
        Json::Value j;
        j["removed"] = (rc == 0);
        SendJson(response, j);
        return;
    }


    // ----- 静态 HTML（monitor.html 等来自 static/）-----
    if (url == "/" || url == "/index.html") {
        std::string body = ReadFile(LunaServerPaths::StaticRoot() + "/index.html");
        if (body.empty())
            body = "<html><body><h3>LunaServer</h3><ul>"
                   "<li><a href=\"/monitor\">monitor</a></li>"
                   "<li><a href=\"/metrics\">metrics</a></li>"
                   "<li><a href=\"/fileserver\">fileserver</a></li>"
                   "<li><a href=\"/gm\">gm</a></li></ul></body></html>";
        SendHtml(response, body, head_only);
        return;
    }
    if (url == "/monitor" || url == "/monitor.html") {
        std::string body = ReadFile(LunaServerPaths::StaticRoot() + "/monitor.html");
        if (body.empty())
            body = "<html><body>缺少 static/monitor.html</body></html>";
        SendHtml(response, body, head_only);
        return;
    }
    if (url == "/fileserver" || url == "/fileserver.html") {
        std::string body = ReadFile(LunaServerPaths::StaticRoot() + "/fileserver.html");
        if (body.empty())
            body = BuildFileHtml();
        SendHtml(response, body, head_only);
        return;
    }
    if (url == "/mhw" || url == "/mhw.html") {
        SendHtml(response, ReadFile(LunaServerPaths::StaticRoot() + "/mhw.html"), head_only);
        return;
    }
    if (url == "/about") {
        SendHtml(response, "<html><body><h3>LunaServer</h3></body></html>", head_only);
        return;
    }

    if (url.size() > 1 && url[0] == '/') {
        std::string path;
        if (!SafeJoinUnderRoot(LunaServerPaths::StaticRoot(), url.substr(1), &path)) {
            response->SetStatusCode(HttpResponse::HttpStatusCode::k404NotFound);
            response->SetStatusMessage("Not Found");
            response->SetCloseConnection(true);
            return;
        }
        std::string body = ReadFile(path);
        if (!body.empty()) {
            SendHtml(response, body, head_only);
            return;
        }
    }

    response->SetStatusCode(HttpResponse::HttpStatusCode::k404NotFound);
    response->SetStatusMessage("Not Found");
    response->SetCloseConnection(true);
}

