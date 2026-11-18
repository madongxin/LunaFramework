#include "bootstrap/PeriodicTasks.h"

#include "bootstrap/LocalRegistry.h"

#include "EventLoop.h"
#include "FormalMode.h"
#include "HealthProbe.h"
#include "Logging.h"
#include "OpsMetrics.h"
#include "PacketTiming.h"
#include "ServiceHealth.h"

#ifdef WEBSERVER_ENABLE_BRPC
#include "GatewayPushClient.h"
#include "IServiceRegistry.h"
#include "RedisServiceRegistry.h"
#include "HealthyLogicOwners.h"
#endif
#ifdef WEBSERVER_ENABLE_GAME_PROTOBUF
#include "GameLogic.h"
#include "MapCatalog.h"
#include "MapLeaseKeeper.h"
#ifdef WEBSERVER_ENABLE_BRPC
#include "MapLineSync.h"
#endif
#endif
#ifdef WEBSERVER_ENABLE_REDIS
#include "PlacementRecoveryScheduler.h"
#include "PlacementStore.h"
#include "SessionStore.h"
#endif
#ifdef WEBSERVER_ENABLE_MYSQL
#include "GameDbOutbox.h"
#ifdef WEBSERVER_ENABLE_GAME_PROTOBUF
#include "FriendStore.h"
#endif
#endif

#include <algorithm>
#include <cstdlib>
#include <string>
#include <vector>

void BootstrapInstallPeriodicTasks(EventLoop *loop, const std::string &role) {
    // 非 gateway 角色：进程内注册续租 + Redis 发现续租
    if (role == "session" || role == "gamelogic" || role == "globalservice" || role == "gamedb") {
        loop->RunEvery(5.0, []() { LocalRegistry::RenewAll(); });
    }
#ifdef WEBSERVER_ENABLE_REDIS
    // Session / Gateway：动态刷新健康 GameLogic Owner 列表
    if (role == "session" || role == "gateway" || role == "all") {
        if (RedisServiceRegistry::Get().ready())
            RefreshHealthyLogicOwners(true);
        loop->RunEvery(5.0, []() {
            if (!RedisServiceRegistry::Get().ready())
                return;
            RefreshHealthyLogicOwners(true);
        });
    }
#endif
#ifdef WEBSERVER_ENABLE_BRPC
    // GameLogic / GlobalService / GameDB / Session：Push 与 KickConnection 都走 gateway_push
    if (role == "gamelogic" || role == "globalservice" || role == "gamedb" || role == "session" ||
        role == "all") {
        loop->RunEvery(5.0, []() {
            if (!RedisServiceRegistry::Get().ready())
                return;
            std::vector<IServiceRegistry::ServiceInstance> insts;
            if (!RedisServiceRegistry::Get().Discover("gateway_push", &insts) || insts.empty())
                return;
            for (const auto &i : insts) {
                if (!i.instance_id.empty() && !i.address.empty())
                    GatewayPushClient::Instance().SetGatewayPushAddr(i.instance_id, i.address);
            }
        });
    }
#endif
#ifdef WEBSERVER_ENABLE_GAME_PROTOBUF
    // GameLogic：Owner lease 续租（默认 lease/3，至少 5s）
    if (role == "gamelogic" || role == "all") {
        uint32_t lease_sec = 30;
#ifdef WEBSERVER_ENABLE_REDIS
        if (PlacementStore::Instance().Available())
            lease_sec = static_cast<uint32_t>(PlacementStore::Instance().default_lease_sec());
#endif
        MapLeaseKeeper::Instance().SetLeaseSec(lease_sec);
        const double hb = std::max(5.0, static_cast<double>(lease_sec) / 3.0);
        loop->RunEvery(hb, []() { MapLeaseKeeper::Instance().Tick(); });
        loop->RunEvery(0.2, []() { GameLogic::Instance().AdvancePresentedMotion(); });
        loop->RunEvery(5.0, []() { GameLogic::Instance().SweepDisconnected(); });
        LOG_INFO << "MapLeaseKeeper interval_sec=" << hb << " lease_sec=" << lease_sec;
    }
#ifdef WEBSERVER_ENABLE_REDIS
    // Session：空线/空本回收（正式常开；与实验热迁恢复解耦）
    if (role == "session" || role == "all") {
#ifdef WEBSERVER_ENABLE_GAME_PROTOBUF
        std::string map_err;
        if (!MapCatalog::Instance().EnsureDefault(&map_err))
            LOG_WARN << "MapCatalog load on session: " << map_err;
        // 故意不在 Session 安装 MapLineSync：Park/Resume 风暴下会 O(occupants) PushBatch
        // 把 Session CPU 打满（本轮压测 sess ~100%），拖死 MarkDisconnected → ONLINE/分线幽灵。
        // 分线推送只由 GameLogic 侧 MapLineSync 负责。
#endif
        if (const char *pfx = std::getenv("LUNASERVER_REDIS_PREFIX"))
            PlacementRecoveryScheduler::Instance().SetKeyPrefix(pfx);
        // 双 Session 时仅 leader 跑 idle/grace/orphan 清扫，避免双倍 Redis 扫描。
        // leader：role=all，或 LUNASERVER_SESSION_SWEEP=1，或 instance_id 缺省/sess-0。
        bool session_sweep_leader = (role == "all");
        if (const char *force = std::getenv("LUNASERVER_SESSION_SWEEP")) {
            session_sweep_leader = (force[0] == '1' || force[0] == 'y' || force[0] == 'Y');
        } else if (!session_sweep_leader) {
            const char *iid = std::getenv("LUNASERVER_INSTANCE_ID");
            session_sweep_leader =
                (iid == nullptr || iid[0] == '\0' || std::string(iid) == "sess-0");
        }
        if (session_sweep_leader) {
            const double idle_iv =
                (std::getenv("LUNASERVER_PLACEMENT_IDLE_IV") != nullptr)
                    ? std::atof(std::getenv("LUNASERVER_PLACEMENT_IDLE_IV"))
                    : 5.0;
            const double iv = idle_iv > 0.5 ? idle_iv : 5.0;
            loop->RunEvery(iv, []() {
                static int sweep_tick = 0;
                ++sweep_tick;
                std::vector<uint64_t> closed;
                if (PlacementStore::Instance().CloseIdleInstances(0, 64, &closed) &&
                    !closed.empty())
                    LOG_INFO << "PlacementIdleCloser: closed idle maps n=" << closed.size();
                const size_t expired = SessionStore::Instance().ExpireDueDisconnected(100);
                const size_t aborted = SessionStore::Instance().ExpireDueTransfers(100);
                if (aborted > 0)
                    LOG_WARN << "SessionTransferSweeper: aborted n=" << aborted;
                if (expired > 0)
                    LOG_INFO << "SessionGraceSweeper: expired n=" << expired;
                // Gateway 进程被杀且 claim 过期后：清幽灵 ONLINE + 分线占位。
                const size_t gw_orphans =
                    SessionStore::Instance().ExpireOnlineMissingGatewayClaim(100);
                if (gw_orphans > 0)
                    LOG_INFO << "SessionGwClaimSweeper: expired n=" << gw_orphans;
                const size_t orphans = SessionStore::Instance().ReclaimOrphanMapReservations(64);
                if (orphans > 0)
                    LOG_INFO << "PlacementOrphanReclaim: released n=" << orphans;
                // occ 对账较重：每 3 个 tick（默认 ~15s）跑一次，避免与 MarkDisconnected 抢 Redis。
                if (sweep_tick % 3 == 0) {
                    const size_t ghost_occ =
                        SessionStore::Instance().ReconcileGhostMapOccupancy(64);
                    if (ghost_occ > 0)
                        LOG_INFO << "PlacementOccReconcile: parked ghost n=" << ghost_occ;
                }
            });
            LOG_INFO << "PlacementIdleCloser interval_sec=" << iv << " leader=1";
        } else {
            LOG_INFO << "PlacementIdleCloser skipped (non-leader session)";
        }
    }
    if (role == "gamelogic" || role == "all") {
#if defined(WEBSERVER_ENABLE_GAME_PROTOBUF) && defined(WEBSERVER_ENABLE_BRPC)
        MapLineSync::Install();
#endif
    }
    // Session：Placement 自动恢复（experimental；正式稳定版默认关闭）
    if ((role == "session" || role == "all") &&
        ExperimentalFeatureEnabled("LUNASERVER_EXPERIMENTAL_PLACEMENT_RECOVERY")) {
        if (const char *env = std::getenv("LUNASERVER_INSTANCE_ID"))
            PlacementRecoveryScheduler::Instance().SetInstanceId(env);
        if (const char *pfx = std::getenv("LUNASERVER_REDIS_PREFIX"))
            PlacementRecoveryScheduler::Instance().SetKeyPrefix(pfx);
        if (const char *sc = std::getenv("LUNASERVER_PLACEMENT_SCAN_COUNT")) {
            const int n = std::atoi(sc);
            if (n > 0)
                PlacementRecoveryScheduler::Instance().SetScanCount(static_cast<size_t>(n));
        }
        const double recover_iv =
            (std::getenv("LUNASERVER_PLACEMENT_RECOVER_IV") != nullptr)
                ? std::atof(std::getenv("LUNASERVER_PLACEMENT_RECOVER_IV"))
                : 5.0;
        const double iv = recover_iv > 0.5 ? recover_iv : 5.0;
        loop->RunEvery(iv, []() { PlacementRecoveryScheduler::Instance().Tick(); });
        LOG_INFO << "PlacementRecoveryScheduler (experimental) interval_sec=" << iv;
    }
#endif
#endif

    // EventLoop 心跳：刷新 /health/live
    loop->RunEvery(1.0, []() { ServiceHealth::Instance().MarkAlive(); });
    loop->RunEvery(10.0, []() { PacketTimingLogWindow(); });
    loop->RunEvery(2.0, [role]() { HealthProbeStore::Instance().Refresh(role); });
    HealthProbeStore::Instance().Refresh(role);
#ifdef WEBSERVER_ENABLE_MYSQL
    if (role == "gamedb" || role == "all") {
        loop->RunEvery(5.0, []() {
            OpsMetrics::Instance().SetOutboxBacklog(
                static_cast<int64_t>(GameDbOutbox::Instance().CountUnpublished()));
        });
#ifdef WEBSERVER_ENABLE_GAME_PROTOBUF
        loop->RunEvery(60.0, []() { FriendStore::Instance().ExpireStaleRequests(200); });
#endif
    }
#endif

}
