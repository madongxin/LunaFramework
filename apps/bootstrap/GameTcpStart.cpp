/**
 * Gateway 玩家面：GameTcpGateway 后台线程 + 本地 MapCatalog / Identity 准备。
 * role=all 时额外起进程内 Transport，便于单进程调试。
 */
#include "bootstrap/GameTcpStart.h"

#include "FormalMode.h"
#include "Logging.h"

#ifdef WEBSERVER_ENABLE_GAME_PROTOBUF
#include "GameTcpGateway.h"
#include "GatewayIdentity.h"
#include "InProcessTransport.h"
#include "MapCatalog.h"
#include "MapInstanceRegistry.h"
#include "MapPlacement.h"
#endif

bool BootstrapStartGameTcp(const std::string &role, int game_port, int http_port,
                           GameTcpGateway **out_gw) {
#ifdef WEBSERVER_ENABLE_GAME_PROTOBUF
    if (out_gw)
        *out_gw = nullptr;
    // 仅 gateway / all 对外开 Game TCP；其它角色直接成功跳过
    if (!(role == "all" || role == "gateway"))
        return true;
    if (game_port <= 0 || game_port > 65535) {
        LOG_ERROR << "Game TCP port invalid: " << game_port;
        return false;
    }
    if (role == "all") {
        InProcessTransport::Instance().EnsureStarted(0);
        MapInstanceRegistry::Instance().SetLocalInstanceId("gl-local");
        MapPlacement::Instance().ConfigureOwners({"gl-local"});
    }
    {
        std::string id_err;
        // gateway_instance_id：Push 定向与 Bind 依赖；解析失败则无法安全启动
        if (!GatewayIdentity::Instance().ready() &&
            !GatewayIdentity::Instance().Resolve(&id_err)) {
            LOG_ERROR << "GatewayIdentity resolve failed: " << id_err;
            return false;
        }
    }
    {
        std::string map_err;
        if (!MapCatalog::Instance().EnsureDefault(&map_err)) {
            LOG_ERROR << "MapCatalog load failed: " << map_err;
            // Formal：地图目录缺失视为硬失败；联调可继续
            if (FormalModeEnabled())
                return false;
        }
    }
    // 玩家口默认 0.0.0.0（公网 VIP/安全组收敛）；管理 HTTP 仍默认 loopback
    auto *gw = new GameTcpGateway("0.0.0.0", game_port, GatewayIdentity::Instance().id());
    gw->StartInBackground();
    LOG_INFO << "Game protobuf TCP " << game_port << " (HTTP " << http_port << ") role=" << role
             << " gateway_instance_id=" << gw->gateway_instance_id();
    if (out_gw)
        *out_gw = gw;
    return true;
#else
    (void)role; (void)game_port; (void)http_port; (void)out_gw;
    return true;
#endif
}

void BootstrapStopGameTcp(GameTcpGateway *&gw) {
#ifdef WEBSERVER_ENABLE_GAME_PROTOBUF
    if (!gw)
        return;
    gw->RequestQuit();
    delete gw;
    gw = nullptr;
#else
    (void)gw;
#endif
}
