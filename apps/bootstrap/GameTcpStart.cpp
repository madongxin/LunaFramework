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
    if (!(role == "all" || role == "gateway"))
        return true;
    if (role == "all") {
        InProcessTransport::Instance().EnsureStarted(0);
        MapInstanceRegistry::Instance().SetLocalInstanceId("gl-local");
        MapPlacement::Instance().ConfigureOwners({"gl-local"});
    }
    {
        std::string id_err;
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
            if (FormalModeEnabled())
                return false;
        }
    }
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
