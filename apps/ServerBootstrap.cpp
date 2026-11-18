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

AsyncLogging *asynclog = nullptr;
std::string g_process_role;
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

EventLoop *g_main_loop = nullptr;

}  // namespace

bool ParseLaunchArgs(int argc, char *argv[], LaunchOpts *opts) {
    if (!opts)
        return false;
    auto is_role = [](const char *s) {
        return s && (!strcmp(s, "all") || !strcmp(s, "gateway") || !strcmp(s, "gamelogic") ||
                     !strcmp(s, "globalservice") || !strcmp(s, "session") || !strcmp(s, "gamedb"));
    };

    std::string role = opts->force_role.empty() ? "all" : opts->force_role;
    int port = opts->http_port > 0 ? opts->http_port : 8080;
    int game_port = opts->game_port;
    int logic_port_override = opts->logic_port_override;

    int argi = 1;
    if (opts->force_role.empty()) {
        if (argc >= 2 && is_role(argv[1])) {
            role = argv[1];
            argi = 2;
        }
    }
    if (argi < argc)
        port = std::atoi(argv[argi++]);
    if (argi < argc) {
        if (role == "gamelogic" || role == "session" || role == "gamedb")
            logic_port_override = std::atoi(argv[argi]);
        else if (role != "globalservice")
            game_port = std::atoi(argv[argi]);
    } else if (game_port <= 0 && role != "gamelogic" && role != "globalservice" && role != "session" &&
               role != "gamedb") {
        game_port = port + 1;
    }

    opts->role = role;
    opts->http_port = port;
    opts->game_port = game_port;
    opts->logic_port_override = logic_port_override;
    return true;
}

int RunServer(const LaunchOpts &launch) {
    LaunchOpts opts = launch;
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
        LOG_WARN << "!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!";
        LOG_WARN << " LUNASERVER_FORMAL=1: fail-closed ownership enforced";
        LOG_WARN << " Auth requires GameDB; Register via Auth→GameDB only";
        LOG_WARN << " Admin HTTP disabled unless LUNASERVER_ENABLE_ADMIN=1";
        LOG_WARN << "!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!";
    }
    if (role == "all" && FormalModeEnabled()) {
        LOG_WARN << "role=all under FORMAL is for emergency only; prefer multi-process topology";
    }

    if (!BootstrapPrepareRole(role, port))
        return 1;

    EventLoop loop;
    g_main_loop = &loop;
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

    BootstrapInitMysql(&loop, role);
    BootstrapInitRedis(role);
    BootstrapInitRocksdb(role);
    ResourceWatchdog::Instance().StartPeriodic(&loop, 5.0);

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
    server.SetHttpCallback(HttpResponseCallback);
    server.SetThreadNums(size);
    LOG_INFO << "HTTP admin bind=" << http_bind << ":" << port
             << " admin_apis=" << (AdminHttpEnabled() ? "on" : "off");

    RoleStartOpts role_opts;
    role_opts.role = role;
    role_opts.http_port = port;
    role_opts.game_port = game_port;
    role_opts.logic_port_override = opts.logic_port_override;
    if (BootstrapStartRoleServices(&loop, role_opts) != 0)
        return 1;

    BootstrapInstallPeriodicTasks(&loop, role);

    GameTcpGateway *game_tcp_gw = nullptr;
    if (!BootstrapStartGameTcp(role, game_port, port, &game_tcp_gw))
        return 1;

#ifdef WEBSERVER_ENABLE_BRPC
    if (role == "all") {
        if (!BrpcGameServer::Instance().StartFromConfig())
            LOG_WARN << "brpc MailBrpcService disabled (config/brpc.cnf or bind failed)";
    }
#endif

    ServiceHealth::Instance().SetReady(true);
    ServiceHealth::Instance().MarkAlive();
    server.start();

    BootstrapGracefulShutdown(role, game_tcp_gw);
    return 0;
}
