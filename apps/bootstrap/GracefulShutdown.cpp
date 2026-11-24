/**
 * Loop 退出后的停服收尾。不负责 EventLoop::Loop（由 HttpServer→TcpServer 主循环承担）。
 */
#include "bootstrap/GracefulShutdown.h"

#include "bootstrap/GameTcpStart.h"
#include "bootstrap/LocalRegistry.h"

#include "Logging.h"
#include "ServiceHealth.h"

#include <chrono>
#include <cstdlib>
#include <thread>

#ifdef WEBSERVER_ENABLE_GAME_PROTOBUF
#include "FriendService.h"
#include "GameLogic.h"
#include "GameTcpGateway.h"
#ifdef WEBSERVER_ENABLE_BRPC
#include "RpcOffloadPool.h"
#endif
#endif
#ifdef WEBSERVER_ENABLE_BRPC
#include "GameDbBrpcServer.h"
#include "GameLogicBrpcServer.h"
#include "GatewayPushServer.h"
#include "GlobalServiceBrpcServer.h"
#include "SessionBrpcServer.h"
#endif

void BootstrapGracefulShutdown(const std::string &role, GameTcpGateway *&game_tcp_gw) {
    // 先宣告 draining：/ready 变坏，拒绝新登录/进图（配合 LoginAdmission 等）
    ServiceHealth::Instance().SetDraining(true);
    ServiceHealth::Instance().SetReady(false);
    LOG_INFO << "graceful shutdown begin role=" << role;
#ifdef WEBSERVER_ENABLE_GAME_PROTOBUF
    if (role == "globalservice" || role == "all" || role == "gamelogic")
        FriendService::Instance().StopPresenceFanout();
    if (game_tcp_gw)
        game_tcp_gw->StopAccepting();  // 停 listen，已有连接仍可收尾
#endif
#ifdef WEBSERVER_ENABLE_BRPC
    LocalRegistry::MarkDraining();
#endif
    {
        // 给负载均衡 / 上游一点时间感知 not-ready；可用环境变量覆盖，最长 60s
        int drain_sec = 2;
        if (const char *e = std::getenv("LUNASERVER_DRAIN_SEC")) {
            const int v = std::atoi(e);
            if (v >= 0 && v <= 60)
                drain_sec = v;
        }
        if (drain_sec > 0) {
            LOG_INFO << "drain wait_sec=" << drain_sec << " (reject new login/enter_map)";
            std::this_thread::sleep_for(std::chrono::seconds(drain_sec));
        }
    }
#ifdef WEBSERVER_ENABLE_BRPC
    LocalRegistry::UnregisterAll();
#ifdef WEBSERVER_ENABLE_GAME_PROTOBUF
    if (role == "gamelogic" || role == "all")
        GameLogic::Instance().FlushAllLastSafe("graceful_shutdown");
    GameLogic::Instance().StopAoiPublisher();
    if (role == "gamelogic")
        RpcOffloadPool::Instance().Stop();
#endif
    // 按角色停对应 brpc；role=all 全停
    if (role == "session" || role == "all")
        SessionBrpcServer::Instance().Stop();
    if (role == "gamelogic" || role == "all")
        GameLogicBrpcServer::Instance().Stop();
    if (role == "globalservice" || role == "all")
        GlobalServiceBrpcServer::Instance().Stop();
    if (role == "gamedb" || role == "all")
        GameDbBrpcServer::Instance().Stop();
    if (role == "gateway" || role == "all")
        GatewayPushServer::Instance().Stop();
#endif
    BootstrapStopGameTcp(game_tcp_gw);
    LOG_INFO << "graceful shutdown done role=" << role;
}
