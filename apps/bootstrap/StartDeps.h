#pragma once

#include <string>

class EventLoop;

/**
 * 启动依赖初始化（在创建主 EventLoop 之后、起 HTTP/brpc 之前）。
 * 与 StartRoles（起各角色 brpc 服务）分工：这里只准备 MySQL/Redis/RocksDB 与角色门禁。
 */

/** AOI 回调、Formal 禁非 gamedb 直连 MySQL、ServiceHealth；缺 brpc 时角色启动失败。 */
bool BootstrapPrepareRole(const std::string &role, int http_port);

/** MySQL 连接池上的 Store / Metrics / Mail；Formal 下非 gamedb 会跳过。 */
void BootstrapInitMysql(EventLoop *loop, const std::string &role);

/** Redis：SessionStore / PlacementStore / PushReplay + 可选 Registry。 */
void BootstrapInitRedis(const std::string &role);

/** RocksDB demo KV（仅 role=all）。 */
void BootstrapInitRocksdb(const std::string &role);
