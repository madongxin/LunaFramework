#pragma once

#include <string>

class GameTcpGateway;

void BootstrapGracefulShutdown(const std::string &role, GameTcpGateway *&game_tcp_gw);
