#pragma once

#include <string>

class GameTcpGateway;

/**
 * Gateway / role=all：拉起对客户端的 Game TCP（ProtoFraming）。
 * 其它角色不应调用成功路径（内部直接跳过或 no-op）。
 */

/** 启动 GameTcp；硬失败返回 false。成功时 *out_gw 指向网关对象（所有权仍由内部管理）。 */
bool BootstrapStartGameTcp(const std::string &role, int game_port, int http_port,
                           GameTcpGateway **out_gw);

/** 优雅退出时停止 Game TCP，并将 gw 置空。 */
void BootstrapStopGameTcp(GameTcpGateway *&gw);
