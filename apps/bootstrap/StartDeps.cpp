/**
 * 启动依赖：角色门禁（PrepareRole）+ MySQL / Redis / RocksDB 初始化。
 * 在主 EventLoop 创建之后、角色 brpc / 管理 HTTP 之前由 RunServer 调用。
 */
#include "bootstrap/StartDeps.h"

#include "AoiEvidence.h"
#include "EventLoop.h"
#include "FormalMode.h"
#include "Logging.h"
#include "ServiceHealth.h"

#ifdef WEBSERVER_ENABLE_MYSQL
#include "ConnectionPool.h"
#include "MetricsDbWriter.h"
#include "PlayerItemPersistQueue.h"
#include "PlayerItemStore.h"
#include "PlayerAccountStore.h"
#include "PlayerProfileStore.h"
#include "GameDbAssetStore.h"
#include "GameDbOutbox.h"
#ifdef WEBSERVER_ENABLE_GAME_PROTOBUF
#include "MailExpireScanner.h"
#include "MailService.h"
#endif
#endif
#ifdef WEBSERVER_ENABLE_GAME_PROTOBUF
#include "GameLogic.h"
#include "GatewayConnRegistry.h"
#include "MapRuntime.h"
#endif
#ifdef WEBSERVER_ENABLE_REDIS
#include "PlacementStore.h"
#include "PushReplayStore.h"
#include "SessionStore.h"
#ifdef WEBSERVER_ENABLE_BRPC
#include "AuthLoginRateLimit.h"
#include "RedisServiceRegistry.h"
#endif
#endif
#ifdef WEBSERVER_ENABLE_ROCKSDB
#include "DemoKvStore.h"
#endif

#include <cstdlib>
#include <string>


// ---------------------------------------------------------------------------
// BootstrapPrepareRole：AOI 钩子、Formal 禁 MySQL、ServiceHealth、brpc 编译检查
// ---------------------------------------------------------------------------
bool BootstrapPrepareRole(const std::string &role, int http_port) {
#ifdef WEBSERVER_ENABLE_GAME_PROTOBUF
    // Gateway：AOI 证据需要 connection→player；从本机 ConnRegistry 查
    if (role == "gateway" || role == "all") {
        AoiSetConnPlayerFn([](uint64_t conn, uint64_t *player) {
            GatewayConnRegistry::Bind bind;
            if (!GatewayConnRegistry::Instance().FindByConnection(conn, &bind))
                return false;
            if (player)
                *player = bind.player_id;
            return true;
        });
    }
    // GameLogic：判断 viewer 是否仍挂在本机地图运行时
    if (role == "gamelogic" || role == "all") {
        AoiSetViewerOnlineFn([](uint64_t id, bool *online) {
            if (online)
                *online = MapRuntime::Instance().ViewerConnected(id);
            return true;
        });
    }
    // Formal：好友相关客户端命令默认关闭（走专用策略）
    if (role == "gamelogic" && FormalModeEnabled())
        GameLogic::Instance().SetFriendCommandsEnabled(false);
#endif
#ifdef WEBSERVER_ENABLE_MYSQL
    // Formal：非 gamedb 禁止初始化连接池，资产经 GameDB brpc
    if (!FormalModeAllowsMysql(role)) {
        ::setenv("LUNASERVER_FORBID_MYSQL", "1", 1);
        ConnectionPool::ForbidInit(
            "formal mode: MySQL only on role=gamedb (use GameDB brpc)");
    }
#endif
    // /live /ready 与监控页用的服务名
    ServiceHealth::Instance().Configure(role, role + "-" + std::to_string(http_port));
#if !defined(WEBSERVER_ENABLE_BRPC)
    if (role == "gateway" || role == "gamelogic" || role == "globalservice" || role == "session" ||
        role == "gamedb") {
        LOG_ERROR << "role=" << role << " requires cmake -DENABLE_BRPC=ON";
        return false;
    }
#endif
    return true;
}


// ---------------------------------------------------------------------------
// MySQL：仅 Formal 允许的角色建池上 Store；gamedb 负责账号/资产/邮件过期扫描
// ---------------------------------------------------------------------------
void BootstrapInitMysql(EventLoop *loop, const std::string &role) {
#ifdef WEBSERVER_ENABLE_MYSQL
    const bool need_db = FormalModeAllowsMysql(role) &&
                         (role == "all" || role == "gamelogic" || role == "globalservice" || role == "gamedb");
    if (FormalModeEnabled() && !FormalModeAllowsMysql(role)) {
        LOG_INFO << "formal mode: skip MySQL pool/stores for role=" << role
                 << " (assets/mail via GameDB)";
    } else if (need_db && ConnectionPool::getconnectionPool()->isInitialized()) {
        if (role != "gamedb")
            MetricsDbWriter::Instance().StartPeriodic(loop, 10.0);
        if (role != "gamedb" && role != "globalservice") {
            PlayerItemStore::Instance().EnsureTable();
            PlayerAccountStore::Instance().EnsureTable();
            PlayerItemPersistQueue::Instance().StartPeriodic(loop, 300.0);
        } else if (role == "globalservice") {
            PlayerItemStore::Instance().EnsureTable();
        } else if (role == "gamedb") {
            PlayerAccountStore::Instance().EnsureTable();
            PlayerProfileStore::Instance().EnsureTable();
            PlayerItemStore::Instance().EnsureTable();
            GameDbAssetStore::Instance().EnsureTables();
            GameDbOutbox::Instance().EnsureTable();
        }
#ifdef WEBSERVER_ENABLE_GAME_PROTOBUF
        if (role == "all" || role == "gamelogic" || role == "gamedb") {
            if (MailService::Instance().Init()) {
                if (role == "all" || role == "gamedb")
                    MailExpireScanner::Instance().StartPeriodic(loop, 0.0);
            } else {
                LOG_WARN << "MailService init failed";
            }
        }
#endif
    } else if (need_db) {
        LOG_WARN << "MySQL pool not initialized (config/mysql.cnf)";
    }
#else
    (void)loop;
    (void)role;
#endif
}


// ---------------------------------------------------------------------------
// Redis：Session / Placement / PushReplay；可选登录限流与服务发现
// ---------------------------------------------------------------------------
bool BootstrapInitRedis(const std::string &role) {
#ifdef WEBSERVER_ENABLE_REDIS
    const bool need_redis =
        (role == "all" || role == "gamelogic" || role == "gateway" || role == "globalservice" ||
         role == "session" || role == "gamedb");
    if (!need_redis)
        return true;
    if (!SessionStore::Instance().InitFromConfig()) {
        // Formal：会话/路由/限流都依赖 Redis，不能带病宣称就绪
        if (FormalModeEnabled()) {
            LOG_ERROR << "formal mode: Redis required for role=" << role
                      << " (config/redis.cnf or connectivity)";
            return false;
        }
        LOG_WARN << "Redis session disabled (config/redis.cnf)";
        return true;
    }
    if (role == "all" || role == "session" || role == "gamelogic" || role == "globalservice" ||
        role == "gateway") {
        PlacementStore::Instance().InitFromSessionPrefix(SessionStore::Instance().key_prefix());
    }
    if (role == "all" || role == "gateway" || role == "gamelogic" || role == "globalservice" ||
        role == "session" || role == "gamedb") {
        PushReplayStore::Instance().InitFromSessionPrefix(SessionStore::Instance().key_prefix());
    }
#ifdef WEBSERVER_ENABLE_BRPC
    AuthLoginRateLimit::Instance().Configure(SessionStore::Instance().key_prefix());
    RedisServiceRegistry::Get().Configure(SessionStore::Instance().key_prefix());
    RedisServiceRegistry::Get().InitFromRedisConfig();
#endif
    return true;
#else
    if (FormalModeEnabled() &&
        (role == "all" || role == "gamelogic" || role == "gateway" || role == "globalservice" ||
         role == "session" || role == "gamedb")) {
        LOG_ERROR << "formal mode: role=" << role << " requires ENABLE_REDIS";
        return false;
    }
    (void)role;
    return true;
#endif
}


// ---------------------------------------------------------------------------
// RocksDB demo（仅 role=all，默认构建通常关闭 ENABLE_ROCKSDB）
// ---------------------------------------------------------------------------
void BootstrapInitRocksdb(const std::string &role) {
#ifdef WEBSERVER_ENABLE_ROCKSDB
    if (role == "all") {
        if (!DemoKvStore::Instance().InitFromConfig())
            LOG_WARN << "RocksDB demo KV disabled (config/rocksdb.cnf)";
    }
#else
    (void)role;
#endif
}
