#include "bootstrap/StartRoles.h"

#include "bootstrap/ConfigKv.h"
#include "bootstrap/LocalRegistry.h"

#include "AdvertiseAddr.h"
#include "EventLoop.h"
#include "FormalMode.h"
#include "LunaServerPaths.h"
#include "Logging.h"

#ifdef WEBSERVER_ENABLE_BRPC
#include "BrpcChannelManager.h"
#include "BrpcGameDbRepository.h"
#include "BrpcTransport.h"
#include "EtcdDiscovery.h"
#include "GameDbBrpcServer.h"
#include "GameLogicBrpcServer.h"
#include "GatewayAuthClients.h"
#include "GatewayConfigPath.h"
#include "GatewayIdentity.h"
#include "GatewayPushClient.h"
#include "GatewayPushServer.h"
#include "GlobalServiceBrpcServer.h"
#include "IServiceRegistry.h"
#include "RedisServiceRegistry.h"
#include "SessionBrpcServer.h"
#include "SessionRpcClient.h"
#ifdef WEBSERVER_ENABLE_GAME_PROTOBUF
#include "MapInstanceRegistry.h"
#ifdef WEBSERVER_ENABLE_MYSQL
#include "MailExpireScanner.h"
#include "MailService.h"
#endif
#endif
#endif

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <string>
#include <vector>

int BootstrapStartRoleServices(EventLoop *loop, const RoleStartOpts &opts) {
#if !defined(WEBSERVER_ENABLE_BRPC)
    (void)loop;
    (void)opts;
    return 0;
#else
    const std::string &role = opts.role;
    const int port = opts.http_port;
    const int game_port = opts.game_port;
    const int logic_port_override = opts.logic_port_override;

    {
        const std::string etcd = GatewayConfigPath::ReadValue("etcd_endpoints");
        if (!etcd.empty())
            EtcdDiscovery::Instance().Configure(etcd);  // 默认 no-op；需 LUNASERVER_ENABLE_ETCD_V2=1
        else
            LOG_INFO << "etcd_endpoints empty; using static *_addrs + brpc list:// naming";
    }

    if (role == "session") {
        if (logic_port_override > 0) {
            char buf[64];
            snprintf(buf, sizeof(buf), "0.0.0.0:%d", logic_port_override);
            if (!SessionBrpcServer::Instance().Start(buf, 30)) {
                LOG_ERROR << "SessionBrpcServer start failed";
                return 1;
            }
        } else if (!SessionBrpcServer::Instance().StartFromConfig()) {
            LOG_ERROR << "SessionBrpcServer start failed";
            return 1;
        }
        const std::string listen = SessionBrpcServer::Instance().listen_addr();
        const int brpc_port = PortFromHostPort(listen);
        const std::string sid =
            LocalRegistry::ResolveInstanceId("session", brpc_port > 0 ? brpc_port : logic_port_override, nullptr);
        const std::string adv = AdvertiseFromListen(listen);
        IServiceRegistry::ServiceInstance inst;
        inst.service = "session";
        inst.instance_id = sid;
        inst.address = adv;
        inst.port = brpc_port;
        inst.status = "UP";
        inst.protocol = "baidu_std";
        if (StaticServiceRegistry::Get().RegisterInstance(inst, 30)) {
            LocalRegistry::Track("session", sid, 30);
            LOG_INFO << "session registered id=" << sid << " advertise=" << adv;
        }
        if (RedisServiceRegistry::Get().ready())
            RedisServiceRegistry::Get().RegisterInstance(inst, 30);
        if (EtcdDiscovery::Instance().enabled())
            EtcdDiscovery::Instance().Register("session", sid, adv, 30);
        ApplyGatewayPushAddrsFromConfig();
        {
            std::string gamedb_addr = GatewayConfigPath::ReadValue("gamedb_addrs");
            if (gamedb_addr.empty()) {
                std::string cnf = "../config/session.cnf";
                std::string resolved;
                if (LunaServerPaths::ResolveProjectSubdir("config/session.cnf", &resolved))
                    cnf = resolved;
                gamedb_addr = LoadCnfValue(cnf, "gamedb_addrs");
            }
            if (!gamedb_addr.empty())
                BrpcGameDbRepository::Instance().Init(SplitAddrList(gamedb_addr));
        }
        LOG_INFO << "role=session listen=" << listen << " instance_id=" << sid;
    } else if (role == "gamedb") {
        if (logic_port_override > 0) {
            char buf[64];
            snprintf(buf, sizeof(buf), "0.0.0.0:%d", logic_port_override);
            if (!GameDbBrpcServer::Instance().Start(buf, 30)) {
                LOG_ERROR << "GameDbBrpcServer start failed";
                return 1;
            }
        } else if (!GameDbBrpcServer::Instance().StartFromConfig()) {
            LOG_ERROR << "GameDbBrpcServer start failed";
            return 1;
        }
        const std::string listen = GameDbBrpcServer::Instance().listen_addr();
        const int brpc_port = PortFromHostPort(listen);
        const char *fb =
            (logic_port_override == 8501 || logic_port_override <= 0) ? "gamedb-0" : "gamedb-1";
        const std::string gid = LocalRegistry::ResolveInstanceId("gamedb", brpc_port, fb);
        const std::string adv = AdvertiseFromListen(listen);
        IServiceRegistry::ServiceInstance inst;
        inst.service = "gamedb";
        inst.instance_id = gid;
        inst.address = adv;
        inst.port = brpc_port;
        inst.status = "UP";
        if (StaticServiceRegistry::Get().RegisterInstance(inst, 30))
            LocalRegistry::Track("gamedb", gid, 30);
        if (RedisServiceRegistry::Get().ready())
            RedisServiceRegistry::Get().RegisterInstance(inst, 30);
        if (EtcdDiscovery::Instance().enabled())
            EtcdDiscovery::Instance().Register("gamedb", gid, adv, 30);
        {
            std::string session_addr = GatewayConfigPath::ReadValue("session_addrs");
            if (session_addr.empty()) {
                std::string logic_cnf = "../config/gamelogic.cnf";
                std::string resolved;
                if (LunaServerPaths::ResolveProjectSubdir("config/gamelogic.cnf", &resolved))
                    logic_cnf = resolved;
                session_addr = LoadCnfValue(logic_cnf, "session_addrs");
            }
            if (!session_addr.empty())
                SessionRpcClient::Instance().Init(session_addr);
        }
        ApplyGatewayPushAddrsFromConfig();
        LOG_INFO << "role=gamedb listen=" << listen << " instance_id=" << gid
                 << " advertise=" << adv;
    } else if (role == "gamelogic") {
        std::string listen = "127.0.0.1:8201";
        if (logic_port_override > 0) {
            char buf[64];
            snprintf(buf, sizeof(buf), "127.0.0.1:%d", logic_port_override);
            listen = buf;
        }
        if (logic_port_override > 0) {
            // 必须尊重 LUNASERVER_INSTANCE_ID（start_formal/e2e 高端口 ≠ 8201）
            const char *fb = (logic_port_override == 8201) ? "gl-0" : "gl-1";
            const std::string iid =
                LocalRegistry::ResolveInstanceId("gl", logic_port_override, fb);
            MapInstanceRegistry::Instance().SetLocalInstanceId(iid);
            if (FormalModeEnabled())
                MapInstanceRegistry::Instance().SetRequireLease(true);
            LOG_INFO << "gamelogic local instance_id=" << iid
                     << " listen_override=" << logic_port_override
                     << " require_lease=" << MapInstanceRegistry::Instance().require_lease();
            GameLogicBrpcServer::Instance().LoadMapRuntimeFromConfig();
            if (!GameLogicBrpcServer::Instance().Start(listen, 30)) {
                LOG_ERROR << "GameLogicBrpcServer start failed";
                return 1;
            }
        } else if (!GameLogicBrpcServer::Instance().StartFromConfig()) {
            LOG_ERROR << "GameLogicBrpcServer start failed";
            return 1;
        }
        {
            std::string session_addr = GatewayConfigPath::ReadValue("session_addrs");
            if (session_addr.empty()) {
                std::string logic_cnf = "../config/gamelogic.cnf";
                std::string resolved;
                if (LunaServerPaths::ResolveProjectSubdir("config/gamelogic.cnf", &resolved))
                    logic_cnf = resolved;
                session_addr = LoadCnfValue(logic_cnf, "session_addrs");
            }
            if (!session_addr.empty())
                SessionRpcClient::Instance().Init(session_addr);
            std::string gamedb_addr = GatewayConfigPath::ReadValue("gamedb_addrs");
            if (gamedb_addr.empty()) {
                std::string logic_cnf = "../config/gamelogic.cnf";
                std::string resolved;
                if (LunaServerPaths::ResolveProjectSubdir("config/gamelogic.cnf", &resolved))
                    logic_cnf = resolved;
                gamedb_addr = LoadCnfValue(logic_cnf, "gamedb_addrs");
            }
            if (!gamedb_addr.empty()) {
                auto addrs = SplitAddrList(gamedb_addr);
                BrpcGameDbRepository::Instance().Init(addrs);
            }
            ApplyGatewayPushAddrsFromConfig();
            std::string logic_listen = GameLogicBrpcServer::Instance().listen_addr();
            const std::string lid = MapInstanceRegistry::Instance().local_instance_id();
            const std::string adv = AdvertiseFromListen(logic_listen);
            IServiceRegistry::ServiceInstance inst;
            inst.service = "gamelogic";
            inst.instance_id = lid.empty() ? LocalRegistry::ResolveInstanceId("gl", PortFromHostPort(logic_listen),
                                                                "gl-0")
                                           : lid;
            inst.address = adv;
            inst.port = PortFromHostPort(logic_listen);
            inst.status = "UP";
            if (StaticServiceRegistry::Get().RegisterInstance(inst, 30))
                LocalRegistry::Track("gamelogic", inst.instance_id, 30);
            if (RedisServiceRegistry::Get().ready())
                RedisServiceRegistry::Get().RegisterInstance(inst, 30);
            if (EtcdDiscovery::Instance().enabled())
                EtcdDiscovery::Instance().Register("gamelogic", inst.instance_id, adv, 30);
            LOG_INFO << "gamelogic registered id=" << inst.instance_id << " advertise=" << adv;
        }
    } else if (role == "gateway") {
        std::vector<std::string> addrs;
        std::vector<std::string> ids;
        std::vector<std::string> globalservice_addrs;
        int timeout_ms = 3000;
        if (!GatewayConfigPath::Load(&addrs, &ids, &globalservice_addrs, &timeout_ms)) {
            addrs = {"127.0.0.1:8201"};
            ids = {"gl-0"};
            globalservice_addrs = {"127.0.0.1:8301"};
            LOG_WARN << "gateway.cnf missing, default logic+globalservice addrs";
        }
        std::vector<std::string> session_addrs;
        std::vector<std::string> session_ids;
        {
            std::string session_csv = GatewayConfigPath::ReadValue("session_addrs");
            GatewayConfigPath::SplitCsv(session_csv, &session_addrs);
            std::string sid_csv = GatewayConfigPath::ReadValue("session_instance_ids");
            GatewayConfigPath::SplitCsv(sid_csv, &session_ids);
        }
        // 遗留 etcd v2：仅在显式开启时合并发现结果（不得截首地址）
        if (EtcdDiscovery::Instance().enabled()) {
            std::vector<std::string> discovered;
            if (EtcdDiscovery::Instance().Discover("gamelogic", &discovered) && !discovered.empty()) {
                addrs = discovered;
                LOG_WARN << "legacy etcd v2 gamelogic addrs=" << addrs.size();
            }
            if (EtcdDiscovery::Instance().Discover("globalservice", &discovered) && !discovered.empty()) {
                globalservice_addrs = discovered;
                LOG_WARN << "legacy etcd v2 globalservice addrs=" << globalservice_addrs.size();
            }
            if (EtcdDiscovery::Instance().Discover("session", &discovered) && !discovered.empty()) {
                session_addrs = discovered;
                LOG_WARN << "legacy etcd v2 session addrs=" << session_addrs.size();
            }
        }
        if (!BrpcTransport::Instance().EnsureStarted(addrs, ids, globalservice_addrs, timeout_ms)) {
            LOG_ERROR << "BrpcTransport init failed";
            return 1;
        }
        if (!session_addrs.empty()) {
            SessionRpcClient::Instance().Init(session_addrs, timeout_ms);
            GatewayAuthClients::Instance().InitAuthSession(session_addrs, timeout_ms);
        } else {
            LOG_INFO << "session_addrs empty; gateway uses local SessionStore";
        }
        // Auth/Session 编排客户端 + Logic Bind/Dispatch Channel（复用，非逐请求创建）
        {
            GatewayAuthClients::Instance().InitLogicChannels(addrs, ids, timeout_ms);
            StaticServiceRegistry::Get().SetStaticAddrs("gamelogic", addrs, ids);
            StaticServiceRegistry::Get().SetStaticAddrs("session", session_addrs, session_ids);
            StaticServiceRegistry::Get().SetStaticAddrs("globalservice", globalservice_addrs);
        }
        // 统一 GatewayInstanceId（GameTCP / Push / Session 绑定共用）
        {
            std::string id_err;
            if (!GatewayIdentity::Instance().Resolve(&id_err)) {
                LOG_ERROR << "GatewayIdentity resolve failed: " << id_err;
                return 1;
            }
            if (!GatewayIdentity::Instance().ClaimOrFail(&id_err)) {
                LOG_ERROR << "GatewayIdentity claim failed: " << id_err;
                return 1;
            }
            // claim TTL=120s；周期续租，避免 Session 误判 Gateway 已死而清 ONLINE。
            loop->RunEvery(30.0, []() {
                if (!GatewayIdentity::Instance().RenewClaim())
                    LOG_WARN << "GatewayIdentity RenewClaim failed";
            });
        }
        const std::string &gw_id = GatewayIdentity::Instance().id();
        // GatewayPush 内网口：listen 与 advertise 分离；身份用 gw_id 而非端口拼接
        {
            const int push_port = (game_port > 0 ? game_port : port) + 100;
            char listen_buf[64];
            snprintf(listen_buf, sizeof(listen_buf), "0.0.0.0:%d", push_port);
            GatewayPushServer::Instance().set_gateway_instance_id(gw_id);
            if (!GatewayPushServer::Instance().Start(listen_buf))
                LOG_WARN << "GatewayPushServer start failed " << listen_buf;
            const std::string adv = MakeAdvertiseAddr(push_port);
            if (EtcdDiscovery::Instance().enabled())
                EtcdDiscovery::Instance().Register("gateway_push", gw_id, adv, 30);
            IServiceRegistry::ServiceInstance inst;
            inst.service = "gateway_push";
            inst.instance_id = gw_id;
            inst.address = adv;
            inst.port = push_port;
            inst.status = "UP";
            if (StaticServiceRegistry::Get().RegisterInstance(inst, 30))
                LocalRegistry::Track("gateway_push", gw_id, 30);
            if (RedisServiceRegistry::Get().ready())
                RedisServiceRegistry::Get().RegisterInstance(inst, 30);
            GatewayPushClient::Instance().SetGatewayPushAddr(gw_id, adv);
        }
        {
            const int gp = game_port > 0 ? game_port : port;
            const std::string adv = MakeAdvertiseAddr(gp);
            IServiceRegistry::ServiceInstance inst;
            inst.service = "gateway";
            inst.instance_id = gw_id;
            inst.address = adv;
            inst.port = gp;
            inst.status = "UP";
            if (StaticServiceRegistry::Get().RegisterInstance(inst, 30))
                LocalRegistry::Track("gateway", gw_id, 30);
            if (RedisServiceRegistry::Get().ready())
                RedisServiceRegistry::Get().RegisterInstance(inst, 30);
            if (EtcdDiscovery::Instance().enabled())
                EtcdDiscovery::Instance().Register("gateway", gw_id, adv, 30);
        }
        // 进程内 lease 续租 + 可选 cnf 热加载（无 etcd v3 时的动态发现降级）
        {
            const char *poll_env = std::getenv("LUNASERVER_DISCOVERY_POLL_SEC");
            double poll_sec = poll_env && *poll_env ? std::atof(poll_env) : 5.0;
            if (poll_sec < 1.0)
                poll_sec = 5.0;
            loop->RunEvery(poll_sec, []() {
                LocalRegistry::RenewAll();
            });
            loop->RunEvery(poll_sec, [timeout_ms]() {
                static std::string last_logic;
                static std::string last_sess;
                static std::string last_gamedb;
                std::vector<std::string> logic_addrs, logic_ids, globalservice_addrs, sess_addrs, sess_ids;
                int to = timeout_ms;
                // Redis 动态发现优先；失败再回落 cnf（空发现不覆盖健康 Channel）
                std::vector<IServiceRegistry::ServiceInstance> redis_logic;
                if (RedisServiceRegistry::Get().ready() &&
                    RedisServiceRegistry::Get().Discover("gamelogic", &redis_logic) &&
                    !redis_logic.empty()) {
                    logic_addrs.clear();
                    logic_ids.clear();
                    for (const auto &i : redis_logic) {
                        logic_addrs.push_back(i.address);
                        logic_ids.push_back(i.instance_id);
                    }
                } else if (!GatewayConfigPath::Load(&logic_addrs, &logic_ids, &globalservice_addrs, &to)) {
                    return;
                }
                std::string logic_key;
                for (size_t i = 0; i < logic_addrs.size(); ++i) {
                    if (i)
                        logic_key.push_back('|');
                    logic_key += (i < logic_ids.size() ? logic_ids[i] : "") + "=" + logic_addrs[i];
                }
                if (!logic_addrs.empty() && logic_key != last_logic) {
                    StaticServiceRegistry::Get().SetStaticAddrs("gamelogic", logic_addrs, logic_ids);
                    BrpcChannelManager::Instance().ApplySnapshot(logic_addrs, logic_ids);
                    GatewayAuthClients::Instance().InitLogicChannels(logic_addrs, logic_ids, to);
                    last_logic = logic_key;
                    LOG_INFO << "discovery ApplySnapshot gamelogic n=" << logic_addrs.size();
                }
                std::vector<IServiceRegistry::ServiceInstance> redis_sess;
                if (RedisServiceRegistry::Get().ready() &&
                    RedisServiceRegistry::Get().Discover("session", &redis_sess) &&
                    !redis_sess.empty()) {
                    sess_addrs.clear();
                    sess_ids.clear();
                    for (const auto &i : redis_sess) {
                        sess_addrs.push_back(i.address);
                        sess_ids.push_back(i.instance_id);
                    }
                }
                // 与静态 cnf 求并集：避免 Redis 注册滞后时把双 Session 收成单点
                {
                    std::vector<std::string> static_addrs, static_ids;
                    GatewayConfigPath::SplitCsv(GatewayConfigPath::ReadValue("session_addrs"),
                                                &static_addrs);
                    GatewayConfigPath::SplitCsv(GatewayConfigPath::ReadValue("session_instance_ids"),
                                                &static_ids);
                    for (size_t i = 0; i < static_addrs.size(); ++i) {
                        bool found = false;
                        for (const auto &a : sess_addrs) {
                            if (a == static_addrs[i]) {
                                found = true;
                                break;
                            }
                        }
                        if (!found && !static_addrs[i].empty()) {
                            sess_addrs.push_back(static_addrs[i]);
                            if (i < static_ids.size())
                                sess_ids.push_back(static_ids[i]);
                        }
                    }
                    if (sess_addrs.empty()) {
                        sess_addrs = std::move(static_addrs);
                        sess_ids = std::move(static_ids);
                    }
                }
                std::string sess_key;
                for (size_t i = 0; i < sess_addrs.size(); ++i) {
                    if (i)
                        sess_key.push_back('|');
                    sess_key += (i < sess_ids.size() ? sess_ids[i] : "") + "=" + sess_addrs[i];
                }
                if (!sess_addrs.empty() && sess_key != last_sess) {
                    StaticServiceRegistry::Get().SetStaticAddrs("session", sess_addrs, sess_ids);
                    SessionRpcClient::Instance().Init(sess_addrs, to);
                    GatewayAuthClients::Instance().InitAuthSession(sess_addrs, to);
                    last_sess = sess_key;
                }
                std::vector<std::string> gdb_addrs;
                std::vector<IServiceRegistry::ServiceInstance> redis_gdb;
                if (RedisServiceRegistry::Get().ready() &&
                    RedisServiceRegistry::Get().Discover("gamedb", &redis_gdb) &&
                    !redis_gdb.empty()) {
                    for (const auto &i : redis_gdb)
                        gdb_addrs.push_back(i.address);
                } else {
                    std::string gc = GatewayConfigPath::ReadValue("gamedb_addrs");
                    GatewayConfigPath::SplitCsv(gc, &gdb_addrs);
                }
                std::string gdb_key;
                for (size_t i = 0; i < gdb_addrs.size(); ++i) {
                    if (i)
                        gdb_key.push_back('|');
                    gdb_key += gdb_addrs[i];
                }
                if (!gdb_addrs.empty() && gdb_key != last_gamedb) {
                    BrpcGameDbRepository::Instance().ApplySnapshot(gdb_addrs);
                    last_gamedb = gdb_key;
                }
            });
        }
    } else if (role == "globalservice") {
        {
            std::string gamedb_addr = GatewayConfigPath::ReadValue("gamedb_addrs");
            if (gamedb_addr.empty()) {
                std::string globalservice_cnf = "../config/globalservice.cnf";
                std::string resolved;
                if (LunaServerPaths::ResolveProjectSubdir("config/globalservice.cnf", &resolved))
                    globalservice_cnf = resolved;
                gamedb_addr = LoadCnfValue(globalservice_cnf, "gamedb_addrs");
            }
            if (!gamedb_addr.empty()) {
                auto addrs = SplitAddrList(gamedb_addr);
                BrpcGameDbRepository::Instance().Init(addrs);
            }
        }
        ApplyGatewayPushAddrsFromConfig();
#ifdef WEBSERVER_ENABLE_GAME_PROTOBUF
#ifdef WEBSERVER_ENABLE_MYSQL
        if (MailService::Instance().Init())
            MailExpireScanner::Instance().StartPeriodic(loop, 0.0);
        else
            LOG_WARN << "MailService init failed";
#endif
#endif
        if (!GlobalServiceBrpcServer::Instance().StartFromConfig()) {
            LOG_ERROR << "GlobalServiceBrpcServer start failed";
            return 1;
        }
        const std::string listen = GlobalServiceBrpcServer::Instance().listen_addr();
        const std::string wid =
            LocalRegistry::ResolveInstanceId("globalservice", PortFromHostPort(listen), "globalservice-1");
        const std::string adv = AdvertiseFromListen(listen);
        IServiceRegistry::ServiceInstance inst;
        inst.service = "globalservice";
        inst.instance_id = wid;
        inst.address = adv;
        inst.port = PortFromHostPort(listen);
        inst.status = "UP";
        if (StaticServiceRegistry::Get().RegisterInstance(inst, 30))
            LocalRegistry::Track("globalservice", wid, 30);
        if (EtcdDiscovery::Instance().enabled())
            EtcdDiscovery::Instance().Register("globalservice", wid, adv, 30);
        LOG_INFO << "role=globalservice listen=" << listen << " instance_id=" << wid
                 << " advertise=" << adv;
    }

    return 0;
#endif
}
