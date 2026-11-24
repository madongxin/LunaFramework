#pragma once

/**
 * @file SessionStore.h
 * @brief 玩家会话在 Redis 中的权威存储（连接池 + Lua CAS）
 *
 * 职责边界（与 Auth 分离）：
 * - AuthService：账号校验、access_token（另一套 Redis key）；**不**创建 Session。
 * - SessionStore / SessionService：AcquireSession 后的 fence / ONLINE / DISCONNECTED、
 *   Logic 路由、断线宽限、踢号、切线 transfer。Gateway 登录链是
 *   Auth.Login → Session.AcquireSession → GameLogic.BindPlayer。
 *
 * 状态机（Redis hash 字段 state）：
 *   Offline →（Acquire/Login）→ Online
 *   Online  →（TCP 断）→ Disconnected（宽限 grace_sec，可 Reconnect）
 *   Disconnected →（宽限到期 / ExpireDue*）→ 删 Session（≠ Logout 语义上的主动下线）
 *   Online  →（Logout / Kick 清会话）→ Offline
 *   MarkDisconnected ≠ Logout：前者保留会话等重连；后者主动销会话。
 *
 * 关键字段：fence_token（业务写权限）、generation（踢号/旧连接忽略）、
 * gamelogic_instance_id / map_* / route_version（权威路由，fence CAS 更新）。
 * 配置：config/redis.cnf；权威并发靠 Redis Lua；cfg_mu_ 只保护进程内配置。
 */

#include "game.pb.h"

#include <cstdint>
#include <functional>
#include <mutex>
#include <string>
#include <vector>

/** 会话状态（Redis 字段 state） */
enum class SessionState {
    Offline = 0,
    Online = 1,
    Disconnected = 2,  // 断线宽限中，可带 ticket/fence 重连
    Closing = 3,
};

/** 一条玩家会话在 Redis 中的镜像（HGETALL session:{player_id}） */
struct SessionRecord {
    std::string token;  // fence_token
    std::string session_id;
    uint32_t server_id = 0;
    int64_t login_time_sec = 0;
    std::string device_id;
    SessionState state = SessionState::Offline;
    std::string gateway_id;
    uint64_t connection_id = 0;
    uint64_t generation = 0;
    int64_t disconnect_deadline_sec = 0;  // DISCONNECTED 宽限截止（unix 秒）
    std::string gamelogic_instance_id;
    uint64_t map_instance_id = 0;
    uint64_t map_owner_epoch = 0;
    uint64_t route_version = 0;
};

/** Gateway → Session.AcquireSession 入参（正式登录创建/顶号会话） */
struct AcquireSessionInput {
    uint64_t account_id = 0;
    uint64_t player_id = 0;
    std::string device_id;
    uint32_t server_id = 0;
    uint32_t ttl_sec = 0;  // 0=用配置 default_ttl_sec_
    bool kick_other_device = true;
    std::string gateway_instance_id;
    std::string preferred_gamelogic_instance_id;
    /** 非空时 Redis 幂等：同 operation_id 重试返回同一 Session/fence */
    std::string operation_id;
};

enum class SessionOpStatus {
    NotFound = 0,
    Pending = 1,
    Done = 2,
};

/** Gateway ReconnectV2 入参（含可选 operation_id 幂等） */
struct ReconnectSessionInput {
    uint64_t player_id = 0;
    std::string session_id;
    std::string reconnect_ticket;
    std::string gateway_instance_id;
    uint64_t last_server_seq = 0;
    std::string operation_id;
};

/**
 * Acquire / Reconnect / Prepare-Commit 共用出参。
 * kicked_previous / previous_*：顶号时带回旧会话，供 Gateway 踢旧连接或 Bind 失败回滚。
 */
struct AcquireSessionResult {
    bool ok = false;
    std::string message;
    std::string error_code;
    std::string session_id;
    std::string fence_token;
    uint64_t generation = 0;
    std::string gamelogic_instance_id;
    uint64_t map_instance_id = 0;
    uint64_t map_owner_epoch = 0;
    uint64_t route_version = 0;
    bool kicked_previous = false;
    int64_t login_time_sec = 0;
    uint32_t server_id = 0;
    std::string previous_gateway_instance_id;
    std::string previous_session_id;
    uint64_t previous_generation = 0;
    uint64_t previous_connection_id = 0;
    std::string previous_fence_token;
    std::string previous_device_id;
    std::string previous_gamelogic_instance_id;
    uint64_t previous_map_instance_id = 0;
    uint64_t previous_map_owner_epoch = 0;
    uint64_t previous_route_version = 0;
    uint32_t previous_server_id = 0;
    int64_t previous_login_time_sec = 0;
};

/**
 * 进程内单例：所有正式会话读写经此入口（Gateway/SessionService/Logic 共享语义）。
 * Available()==false 时 Formal 应 fail-closed，禁止静默降级到无会话。
 */
class SessionStore {
public:
    static SessionStore &Instance();

    /**
     * 读 config/redis.cnf，初始化 RedisPool 与 key_prefix/grace/ttl。
     * 失败则 Available()==false；正式路径不得继续当会话权威。
     */
    bool InitFromConfig();
    /** Redis 池与配置是否就绪；false 时所有写路径应拒绝。 */
    bool Available() const { return available_; }
    /** MarkDisconnected 后允许 Reconnect 的宽限秒数（来自配置）。 */
    int grace_sec() const { return grace_sec_; }
    /** PrepareReconnect 候选 fence 的 Redis TTL。超期未 Commit 视为 Abort。 */
    static constexpr int kReconnectPrepareTtlSec = 10;
    /** BeginPlayerTransfer 后未 Commit/Abort 的最长存活秒数。 */
    static constexpr int kTransferTtlSec = 30;
    /** Redis key 前缀（如 lunaserver:dev:）；Placement/PushReplay 与之对齐。 */
    const std::string &key_prefix() const { return key_prefix_; }

    /**
     * 配置可分配的 GameLogic instance_id 列表（勿写死端口）。
     * 空列表时 Acquire 失败；publish_snapshot 可推健康 Logic 快照。
     */
    void SetLogicInstanceIds(std::vector<std::string> ids, bool publish_snapshot = true);
    /** 当前可分配 Logic 实例列表的拷贝（受 cfg_mu_ 保护）。 */
    std::vector<std::string> LogicInstanceIds() const;

    // ----- 登录 / 重连 / 下线（核心会话生命周期）-----

    /**
     * 正式路径：创建或顶号会话，分配 fence + Logic 路由。
     * kick_other_device 时旧会话写入 previous_*；operation_id 非空则 Redis 幂等。
     */
    bool AcquireSession(const AcquireSessionInput &in, AcquireSessionResult *out);
    /**
     * 旧 proto Login 封装（内部仍写 Session）。
     * 正式 Gateway 应走 Auth.Login → AcquireSession，勿让客户端直达此路径。
     */
    bool Login(const game::LoginReq &req, game::LoginRsp *rsp);
    /**
     * 断线宽限内重连：校验 session_id/ticket，恢复 ONLINE 并换新 fence。
     * 宽限已过或校验失败时 out->ok=false。
     */
    bool Reconnect(const game::ReconnectReq &req, game::ReconnectRsp *rsp);
    /** 同上，并额外写出权威路由字段到 *route_out（供 Gateway Bind / 粘性转发）。 */
    bool Reconnect(const game::ReconnectReq &req, game::ReconnectRsp *rsp, SessionRecord *route_out);
    /**
     * Gateway ReconnectV2：带 operation_id 幂等的重连。
     * 同 operation_id 重试返回同一 fence/generation，避免双绑。
     */
    bool ReconnectSession(const ReconnectSessionInput &in, AcquireSessionResult *out,
                          SessionRecord *route_out);
    /**
     * 两阶段重连：Prepare 写出候选 fence（短 TTL）→ Gateway Bind 成功后再 Commit。
     * 失败或超时应 Abort，避免半开会话。
     */
    bool PrepareReconnect(const ReconnectSessionInput &in, AcquireSessionResult *out);
    bool CommitReconnect(uint64_t player_id, const std::string &operation_id,
                         const std::string &candidate_fence, uint64_t candidate_generation,
                         const std::string &gateway_instance_id, uint64_t connection_id,
                         AcquireSessionResult *out);
    bool AbortReconnect(uint64_t player_id, const std::string &operation_id,
                        const std::string &candidate_fence);
    /** 查询幂等操作结果（超时后结果未知） */
    bool GetSessionOperation(const std::string &operation_id, SessionOpStatus *status,
                             std::string *op_kind, AcquireSessionResult *out);
    /** 校验 session_id/token 是否仍有效（调试/旧路径；业务写优先 ValidateToken）。 */
    bool Validate(const game::ValidateSessionReq &req, game::ValidateSessionRsp *rsp);
    /** 查询玩家是否 ONLINE（不含 DISCONNECTED 宽限态）。 */
    bool CheckOnline(const game::CheckOnlineReq &req, game::CheckOnlineRsp *rsp);
    /** 主动下线：删 Session；与 MarkDisconnected（断线宽限）不同。 */
    bool Logout(const game::LogoutReq &req, game::LogoutRsp *rsp);

    /** 仅 ONLINE 且 token 匹配才通过（DISCONNECTED 拒业务写） */
    bool ValidateToken(uint64_t player_id, const std::string &token, std::string *err);
    bool IsPlayerOnline(uint64_t player_id);

    /** Gateway 在 Login/Reconnect 成功回包后绑定连接 */
    bool BindConnection(uint64_t player_id, const std::string &token, const std::string &gateway_id,
                        uint64_t connection_id);
    /**
     * 断线：token+generation 匹配才进入 DISCONNECTED + 宽限期。
     * 旧连接迟到回调因 generation/token 不匹配而忽略。
     * 不等于 Logout。
     */
    bool MarkDisconnected(uint64_t player_id, const std::string &token, uint64_t generation);

    // ----- Session 进程定时清理 / 对账（leader 侧调用）-----

    /**
     * 扫描宽限已到期的 DISCONNECTED 会话：删 Session、ReleaseByPlayer、从分线占位摘除。
     * 不等于 Logout；只处理 deadline 已过的断线号。Session 进程定时调用。
     * 只处理 score ≤ now 的成员，单次最多 limit 条。
     */
    size_t ExpireDueDisconnected(size_t limit = 100);
    /** 转移记录超过 kTransferTtlSec 未 Commit 时回滚为 ONLINE。 */
    size_t ExpireDueTransfers(size_t limit = 100);
    /**
     * 释放没有 ONLINE/DISCONNECTED Session 的地图占位（切线失败、进程被杀留下的幽灵 occupancy）。
     */
    size_t ReclaimOrphanMapReservations(size_t limit = 64);
    /** map:occ 对账：非 ONLINE Session 立刻摘除 softcap。 */
    size_t ReconcileGhostMapOccupancy(size_t limit = 128);
    /**
     * 集群冷杀/Gateway 宕机后：ONLINE 且 gatewayId 非空，但 `gw_claim:{gw}` 已不存在时，
     * 删 Session、UntrackOnline、ReleaseByPlayer（不等于 Logout，也无重连宽限——对端 Gateway 已死）。
     * Session leader 定时调用；单次最多 limit 条。
     */
    size_t ExpireOnlineMissingGatewayClaim(size_t limit = 100);

    struct KickResult {
        bool ok = false;
        std::string message;
        std::string error_code;
        uint64_t new_generation = 0;
        uint64_t old_generation = 0;
        std::string old_gateway_id;
        std::string old_session_id;
        std::string old_token;
    };
    /** 顶号/踢人：CAS 抬升 generation 并使旧 fence 失效。 */
    bool Kick(uint64_t player_id, const std::string &reason, std::string *err);
    bool Kick(uint64_t player_id, const std::string &reason, KickResult *out);

    /**
     * Bind 失败补偿：仅当 Redis 仍是本次 Acquire 的 new fence 时恢复旧会话。
     * previous_fence 为空则删除新会话。operation_id 非空时删除幂等 DONE，允许重试。
     */
    bool RestorePreviousSession(uint64_t player_id, const std::string &current_fence,
                                const AcquireSessionResult &previous, const std::string &operation_id,
                                std::string *err);
    bool InvalidateOperation(const std::string &operation_id);

    // ----- 路由 / 切线（GameLogic 权威，fence CAS）-----

    /** 进图/迁移后更新权威路由（fence CAS；route_version 单调） */
    bool UpdatePlayerRoute(uint64_t player_id, const std::string &fence_token,
                           const std::string &gamelogic_instance_id, uint64_t map_instance_id,
                           uint64_t map_owner_epoch, uint64_t route_version,
                           const std::string &gateway_instance_id, const std::string &push_endpoint,
                           uint64_t *route_version_out, std::string *err);

    struct TransferBeginIn {
        uint64_t player_id = 0;
        std::string fence_token;
        uint64_t expected_route_version = 0;
        std::string from_logic;
        std::string to_logic;
        uint64_t map_instance_id = 0;
        uint64_t map_owner_epoch = 0;
        std::string transfer_id;
        std::string gateway_instance_id;
    };
    struct TransferBeginOut {
        bool ok = false;
        std::string message;
        std::string error_code;
        std::string transfer_id;
        uint64_t route_version = 0;
        std::string route_state;
    };
    /**
     * 切线/跨 Logic 迁移第一步：CAS 校验 fence+route_version，写入转移记录（短 TTL）。
     * 成功后会话进入迁移态；须在 kTransferTtlSec 内 Commit 或 Abort。
     */
    bool BeginPlayerTransfer(const TransferBeginIn &in, TransferBeginOut *out);

    struct TransferCommitIn {
        uint64_t player_id = 0;
        std::string fence_token;
        std::string transfer_id;
        std::string to_logic;
        uint64_t map_instance_id = 0;
        uint64_t map_owner_epoch = 0;
        std::string gateway_instance_id;
    };
    struct TransferCommitOut {
        bool ok = false;
        std::string message;
        std::string error_code;
        uint64_t route_version = 0;
        std::string gamelogic_instance_id;
        uint64_t map_instance_id = 0;
        uint64_t map_owner_epoch = 0;
        std::string route_state;
    };
    /**
     * 迁移提交：把权威路由切到 to_logic/map/epoch，抬升 route_version，清转移记录。
     * fence/transfer_id 不匹配则失败，避免半切。
     */
    bool CommitPlayerTransfer(const TransferCommitIn &in, TransferCommitOut *out);
    /**
     * 迁移回滚：恢复转移前 ONLINE 路由；须仍持有同一 fence 与 transfer_id。
     * *route_version_out 为回滚后的权威版本（可空）。
     */
    bool AbortPlayerTransfer(uint64_t player_id, const std::string &fence_token,
                             const std::string &transfer_id, std::string *err,
                             uint64_t *route_version_out);
    /**
     * 带 fence 读权威路由与 route_state/transfer_id。
     * fence 不匹配返回 false（防过期连接读到新路由后乱写）。
     */
    bool GetPlayerRoute(uint64_t player_id, const std::string &fence_token, SessionRecord *out,
                        std::string *route_state, std::string *transfer_id, std::string *err);

    void SetAfterGraceLoadHookForTest(std::function<void()> hook) {
        grace_after_load_hook_ = std::move(hook);
    }
    /** 用过期本地快照走 CAS；Redis 权威记录可能已被重连更新。 */
    bool ForceExpireGraceFromStaleLocalForTest(uint64_t player_id);

    // ----- 在线态 / 推送目标 / 社交旁路缓存（不把 fence 泄漏给公网）-----

    struct OnlinePushTarget {
        uint64_t player_id = 0;
        std::string gateway_id;
        std::string session_id;
        std::string fence_token;
        uint64_t generation = 0;
    };

    /** 聊天广播：仅 ONLINE 且有 gateway/session。不返回 token 给公网。 */
    bool ListOnlinePushTargets(std::vector<OnlinePushTarget> *out, size_t max_n = 256);
    /** 公网在线态：online | offline | disconnected。无会话视为 offline。 */
    bool QueryPublicOnlineState(uint64_t player_id, std::string *state);

    struct PublicPresence {
        std::string state = "offline";
        uint64_t map_instance_id = 0;
        std::string gamelogic_instance_id;
        uint64_t map_owner_epoch = 0;
        /** 在线或重连宽限中：本次登录时间。离线为 0，最近在线另见 BatchLastSeen。 */
        int64_t last_online_unix = 0;
    };
    /** 公网存在感：state + 地图/Logic 摘要 + last_online_unix（离线为 0）。 */
    bool QueryPublicPresence(uint64_t player_id, PublicPresence *out);
    /** 内部读会话（含 fence）；不校验 fence。勿把结果泄漏到公网客户端。 */
    bool PeekSession(uint64_t player_id, SessionRecord *out);
    /** 单玩家 ONLINE 推送目标；离线或不存在返回 false。 */
    bool GetOnlinePushTarget(uint64_t player_id, OnlinePushTarget *out);
    /** Redis `online:players` 对账后的 ONLINE 人数（TTL/幽灵成员会被 SREM）。 */
    int64_t OnlinePlayerCount();
    /** 聊天限流：窗口内超限返回 false（Redis 计数）。 */
    bool ConsumeChatQuota(uint64_t player_id, int limit, int window_sec);
    /** 按名查询限流（搜人/简介等）。 */
    bool ConsumeNameQueryQuota(uint64_t player_id, int limit, int window_sec);
    /** 好友申请限流（固定窗口，配置在实现内）。 */
    bool ConsumeFriendApplyQuota(uint64_t player_id);
    /** 世界聊天消息单调 id（Redis INCR）。 */
    uint64_t NextWorldChatMessageId();

    /** 批量公网存在感；out 与 player_ids 等长，缺席填 offline。 */
    bool BatchQueryPublicPresence(const std::vector<uint64_t> &player_ids,
                                  std::vector<PublicPresence> *out);
    /** 一次 HGETALL 取出仍在线、且带 gateway 的推送目标。presence 不逐个 PeekSession。 */
    bool BatchOnlinePushTargets(const std::vector<uint64_t> &player_ids,
                                std::vector<OnlinePushTarget> *out);
    /** 失败时删除 key，避免半截集合被当成完整好友列表。 */
    bool ReplaceFriendIdCache(uint64_t player_id, const std::vector<uint64_t> &friend_ids);
    /** 接受好友后增量写入旁路缓存。 */
    void AddFriendIdCache(uint64_t player_id, uint64_t friend_id);
    /** 删友后从旁路缓存摘除。 */
    void RemoveFriendIdCache(uint64_t player_id, uint64_t friend_id);
    /** 读旁路好友 id 列表；key 不存在返回 false（需回源 GameDB）。 */
    bool ListFriendIdsFromCache(uint64_t player_id, std::vector<uint64_t> *out);
    /** `friend:block:{id}`。未命中返回 false；已知空集返回 true 且 out 为空。 */
    bool ListBlockedIdsFromCache(uint64_t player_id, std::vector<uint64_t> *out);
    /** 全量替换拉黑旁路缓存（含空集，避免误判未命中）。 */
    bool ReplaceBlockCache(uint64_t player_id, const std::vector<uint64_t> &blocked_ids);
    /** 拉黑变更后删缓存，下次读强制回源。 */
    void InvalidateBlockCache(uint64_t player_id);
    /** 离线后好友列表用的最近在线时间，保留 30 天。 */
    void RememberLastSeen(uint64_t player_id, int64_t unix_sec);
    bool BatchLastSeen(const std::vector<uint64_t> &player_ids, std::vector<int64_t> *out);

    /** 上线/下线时回调（FriendService 注册 presence 扇出）。 */
    using FriendPresenceFn = void (*)(uint64_t player_id, bool online);
    void SetFriendPresenceFn(FriendPresenceFn fn);

private:
    SessionStore() = default;
    std::string SessionKey(uint64_t player_id) const;
    std::string OpKey(const std::string &operation_id) const;
    std::string ReconnectPendingKey(const std::string &operation_id) const;
    bool LoadSession(uint64_t player_id, SessionRecord *out);
    bool ExpireIfGraceElapsed(uint64_t player_id, SessionRecord *rec);
    static std::string StateToString(SessionState s);
    static SessionState StateFromString(const std::string &s);
    std::string OnlineSetKey() const;
    std::string GraceIndexKey() const;
    std::string TransferDeadlineKey() const;
    void IndexGraceDeadline(uint64_t player_id, int64_t deadline_unix);
    void ClearGraceIndex(uint64_t player_id);
    void TrackOnline(uint64_t player_id);
    void UntrackOnline(uint64_t player_id);
    bool ConsumeKeyedQuota(const std::string &key, int limit, int window_sec);

    enum class OpBegin { Execute, Done, Pending, Error };
    OpBegin BeginOperation(const std::string &operation_id, AcquireSessionResult *cached,
                           std::string *op_kind_out, std::string *err);
    bool CompleteOperation(const std::string &operation_id, const std::string &op_kind,
                           const AcquireSessionResult &result);
    bool AbortOperation(const std::string &operation_id);
    bool LoadOperationResult(const std::string &operation_id, SessionOpStatus *status,
                             std::string *op_kind, AcquireSessionResult *out);
    bool AcquireSessionUnlocked(const AcquireSessionInput &in, AcquireSessionResult *out);

    bool available_ = false;
    int default_ttl_sec_ = 7200;
    int long_ttl_sec_ = 86400;
    int grace_sec_ = 45;           // MarkDisconnected 后允许重连的秒数
    int pool_size_ = 8;
    std::string key_prefix_ = "lunaserver:dev:";
    std::vector<std::string> logic_instance_ids_{"gl-0"};
    /** 保护 logic_instance_ids_ 等进程内配置；Redis 权威状态靠 Lua */
    mutable std::mutex cfg_mu_;
    std::function<void()> grace_after_load_hook_;
    FriendPresenceFn friend_presence_fn_ = nullptr;
};
