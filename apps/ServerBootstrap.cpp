/**
 * @file ServerBootstrap.cpp
 * @brief RunServer：日志、依赖、角色启动、管理 HTTP、信号与优雅退出
 */

#include "ServerBootstrap.h"

#include "bootstrap/AdminHttpRouter.h"
#include "bootstrap/BootstrapState.h"
#include "bootstrap/GameTcpStart.h"
#include "bootstrap/GracefulShutdown.h"
#include "bootstrap/PeriodicTasks.h"
#include "bootstrap/StartDeps.h"
#include "bootstrap/StartRoles.h"

#include "AsyncLogging.h"
#include "BindAddressPolicy.h"
#include "EventLoop.h"
#include "FormalMode.h"
#include "HttpServer.h"
#include "Logging.h"
#include "ResourceWatchdog.h"
#include "ServiceHealth.h"

#include <algorithm>
#include <csignal>
#include <cstring>
#include <cstdlib>
#include <string>
#include <sys/stat.h>
#include <thread>

#ifdef WEBSERVER_ENABLE_BRPC
#include "BrpcGameServer.h"
#endif
#ifdef WEBSERVER_ENABLE_GAME_PROTOBUF
#include "GameTcpGateway.h"
#else
class GameTcpGateway;
#endif

// ---------------------------------------------------------------------------
// 进程级共享状态（管理 HTTP / 日志 / 信号回调需要跨函数可见）
// ---------------------------------------------------------------------------

/** 异步日志后端；Logger 的 output/flush 回调写到这里。生命周期与进程同在。 */
AsyncLogging *asynclog = nullptr;
/** 当前进程角色名，供日志标签与部分路由使用。 */
std::string g_process_role;
/**
 * 玩家 Game TCP 监听端口（仅 gateway/all 有意义）。
 * gamelogic/session/gamedb/globalservice 置 0：这些角色不对客户端开 Game TCP。
 */
int g_game_port = 0;

namespace {

void AsyncOutputFunc(const char *msg, int len) {
    if (asynclog)
        asynclog->Append(msg, len);
}

void AsyncFlushFunc() {
    if (asynclog)
        asynclog->Flush();
}

/**
 * 主线程 EventLoop 指针，仅供信号处理器调用 Quit()。
 * std::signal 回调不能可靠捕获局部引用，故用文件内全局。
 */
EventLoop *g_main_loop = nullptr;

}  // namespace

/** 解析 argv 到 opts；见 ServerBootstrap.h 约定 */
bool ParseLaunchArgs(int argc, char *argv[], LaunchOpts *opts) {
    if (!opts)
        return false;
    auto is_role = [](const char *s) {
        return s && (!strcmp(s, "all") || !strcmp(s, "gateway") || !strcmp(s, "gamelogic") ||
                     !strcmp(s, "globalservice") || !strcmp(s, "session") || !strcmp(s, "gamedb"));
    };

    // force_role：专用二进制入口已钉死角色；否则默认 all，或从 argv[1] 读。
    std::string role = opts->force_role.empty() ? "all" : opts->force_role;
    int port = opts->http_port > 0 ? opts->http_port : 8080;
    int game_port = opts->game_port;
    int logic_port_override = opts->logic_port_override;

    int argi = 1;
    if (opts->force_role.empty()) {
        // 兼容：./server gateway 8080 8081
        if (argc >= 2 && is_role(argv[1])) {
            role = argv[1];
            argi = 2;
        }
    }
    // 下一参数：HTTP 管理口
    if (argi < argc)
        port = std::atoi(argv[argi++]);
    // 再下一参数：按角色解释为 Game TCP 或 brpc listen
    if (argi < argc) {
        if (role == "gamelogic" || role == "session" || role == "gamedb")
            logic_port_override = std::atoi(argv[argi]);
        else if (role != "globalservice")
            game_port = std::atoi(argv[argi]);
    } else if (game_port <= 0 && role != "gamelogic" && role != "globalservice" && role != "session" &&
               role != "gamedb") {
        // gateway/all 未显式给 game_port 时：默认 HTTP+1（如 8080 → 8081）
        game_port = port + 1;
    }

    opts->role = role;
    opts->http_port = port;
    opts->game_port = game_port;
    opts->logic_port_override = logic_port_override;
    return true;
}

/** 完整启动：AsyncLogging → Formal/依赖 → 角色服务 → HTTP → loop → 信号退出 */
int RunServer(const LaunchOpts &launch) {
    LaunchOpts opts = launch;

    // ----- 日志：先落盘，后续启动失败也能在 ./log/<basename>.log 看到 -----
    Logger::setLogLevel(Logger::INFO);
    ::mkdir("./log", 0755);
    const std::string log_path = "./log/" + opts.log_basename + ".log";
    asynclog = new AsyncLogging(log_path.c_str());
    asynclog->Start();
    Logger::setOutput(AsyncOutputFunc);
    Logger::setFlush(AsyncFlushFunc);

    const std::string &role = opts.role;
    g_process_role = role;
    const int port = opts.http_port;
    int game_port = opts.game_port;
    // 再兜底一次 game_port（ParseLaunchArgs 之后若调用方只填了部分字段）
    if (game_port <= 0 && role != "gamelogic" && role != "globalservice" && role != "session" &&
        role != "gamedb")
        game_port = port + 1;
    if (role == "gamelogic" || role == "globalservice" || role == "session" || role == "gamedb")
        g_game_port = 0;
    else
        g_game_port = game_port;

    SetLogService(role.c_str());
    LOG_INFO << "lunaserver role=" << role << " http=" << port << " game_port=" << g_game_port;
    if (FormalModeEnabled()) {
        // 醒目提示：正式模式 fail-closed，避免误以为本地降级仍可用
        LOG_WARN << "!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!";
        LOG_WARN << " LUNASERVER_FORMAL=1: fail-closed ownership enforced";
        LOG_WARN << " Auth requires GameDB; Register via Auth→GameDB only";
        LOG_WARN << " Admin HTTP disabled unless LUNASERVER_ENABLE_ADMIN=1";
        LOG_WARN << "!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!";
    }
    if (role == "all" && FormalModeEnabled()) {
        LOG_WARN << "role=all under FORMAL is for emergency only; prefer multi-process topology";
    }

    // 角色门禁：AOI 回调、Formal 禁非 gamedb 直连 MySQL、健康检查身份、brpc 编译检查
    if (!BootstrapPrepareRole(role, port))
        return 1;

    // ----- 主 Reactor：后续 HTTP/定时器/部分业务都挂在这条 EventLoop 上 -----
    EventLoop loop;
    g_main_loop = &loop;
    // SIGTERM（stop 脚本 / kill）与 SIGINT（Ctrl+C）：先 draining 再 Quit，避免硬杀
    std::signal(SIGTERM, [](int) {
        ServiceHealth::Instance().SetDraining(true);
        if (g_main_loop)
            g_main_loop->Quit();
    });
    std::signal(SIGINT, [](int) {
        ServiceHealth::Instance().SetDraining(true);
        if (g_main_loop)
            g_main_loop->Quit();
    });

    // 依赖初始化（Formal 下非 gamedb 会跳过 MySQL 池）
    BootstrapInitMysql(&loop, role);
    BootstrapInitRedis(role);
    BootstrapInitRocksdb(role);
    // 周期性采样 RSS/FD 等，写入指标
    ResourceWatchdog::Instance().StartPeriodic(&loop, 5.0);

    // ----- HTTP 管理口：默认 127.0.0.1；公网需 LUNASERVER_HTTP_BIND + ALLOW_PUBLIC_BIND -----
    const unsigned ncpu = std::thread::hardware_concurrency();
    const int size = std::max(8, ncpu > 0 ? static_cast<int>(ncpu) : 4);
    const char *http_bind_env = std::getenv("LUNASERVER_HTTP_BIND");
    std::string http_bind = (http_bind_env && *http_bind_env) ? http_bind_env : "127.0.0.1";
    if (FormalModeEnabled()) {
        const ListenBindDecision http_bind_decision =
            EvaluateListenBind(http_bind, true, AllowPublicBindFromEnv());
        if (!http_bind_decision.ok) {
            LOG_ERROR << "HTTP admin " << http_bind_decision.error;
            return 1;
        }
    }
    HttpServer server(&loop, http_bind.c_str(), port, true);
    server.SetHttpCallback(HttpResponseCallback);  // AdminHttpRouter：/metrics /monitor /live …
    server.SetThreadNums(size);
    LOG_INFO << "HTTP admin bind=" << http_bind << ":" << port
             << " admin_apis=" << (AdminHttpEnabled() ? "on" : "off");

    // 按角色启动 brpc 服务端/客户端（Auth、Session、GameLogic、GameDB、Push 等）
    RoleStartOpts role_opts;
    role_opts.role = role;
    role_opts.http_port = port;
    role_opts.game_port = game_port;
    role_opts.logic_port_override = opts.logic_port_override;
    if (BootstrapStartRoleServices(&loop, role_opts) != 0)
        return 1;

    // 心跳、指标落库、地图租约等周期任务
    BootstrapInstallPeriodicTasks(&loop, role);

    // 仅 gateway/all：对客户端开 ProtoFraming Game TCP
    GameTcpGateway *game_tcp_gw = nullptr;
    if (!BootstrapStartGameTcp(role, game_port, port, &game_tcp_gw))
        return 1;

#ifdef WEBSERVER_ENABLE_BRPC
    // 单进程 all：额外尝试拉起邮件等旧 BrpcGameServer（多进程拓扑下各角色已有独立 brpc）
    if (role == "all") {
        if (!BrpcGameServer::Instance().StartFromConfig())
            LOG_WARN << "brpc MailBrpcService disabled (config/brpc.cnf or bind failed)";
    }
#endif

    // 依赖与监听就绪后再对外宣称 ready；Prometheus/监控据此判定
    ServiceHealth::Instance().SetReady(true);
    ServiceHealth::Instance().MarkAlive();
    server.start();

    // server.start() 已在 TcpServer::Loop 里阻塞到 Quit；此处做停服收尾
    BootstrapGracefulShutdown(role, game_tcp_gw);
    return 0;
}
