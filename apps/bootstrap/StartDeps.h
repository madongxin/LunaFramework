#pragma once

#include <string>

class EventLoop;

/** AOI hooks, Formal MySQL forbid, ServiceHealth configure. Returns false if role needs brpc but disabled. */
bool BootstrapPrepareRole(const std::string &role, int http_port);

/** MySQL stores / Metrics / Mail init for roles that own DB. */
void BootstrapInitMysql(EventLoop *loop, const std::string &role);

/** Redis Session/Placement/PushReplay + registry. */
void BootstrapInitRedis(const std::string &role);

/** RocksDB demo KV (role=all only). */
void BootstrapInitRocksdb(const std::string &role);
