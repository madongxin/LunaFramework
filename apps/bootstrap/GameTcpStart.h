#pragma once

#include <string>

class GameTcpGateway;

/** Start GameTcp for gateway/all. Returns false on hard failure. */
bool BootstrapStartGameTcp(const std::string &role, int game_port, int http_port,
                           GameTcpGateway **out_gw);

void BootstrapStopGameTcp(GameTcpGateway *&gw);
