#pragma once

/**
 * @file PlacementStore.h
 * @brief 权威 Map Placement（Session 进程内模块；Redis Lua/CAS）
 *
 * Gateway/GameLogic 仅缓存，不以本地 MapPlacement 为事实源。
 * 支持公共池/LINE/DUNGEON 占位、切线、排队、lease 心跳与幽灵 occupancy 对账。
 * 与 SessionStore 共用 RedisPool；在 Session 启动时 InitFromSessionPrefix。
 */

#include "SceneKind.h"

#include <cstdint>
#include <functional>
#include <mutex>
#include <string>
#include <vector>

/** Placement 状态（Redis 字段 state） */
enum class PlacementState {
    Creating = 0,
    Ready = 1,
    Frozen = 2,
    Migrating = 3,
    Recovering = 4,
    Closed = 5,
    Draining = 6,
};

struct PlacementRecord {
    uint32_t realm_id = 0;
    uint64_t map_template_id = 0;
    uint64_t map_instance_id = 0;
    std::string owner_logic_server_id;
    uint64_t owner_epoch = 0;
    uint64_t route_version = 0;
    PlacementState state = PlacementState::Closed;
    int64_t updated_at = 0;
    int64_t lease_until = 0;
    std::string kind;
    uint32_t line_no = 0;
    uint32_t soft_cap = 0;
    uint32_t hard_cap = 0;
};

struct ResolveOrCreateInput {
    uint32_t realm_id = 0;
    uint64_t map_template_id = 0;
    uint64_t map_instance_id = 0;
    std::string preferred_owner;
    bool force_new = false;
    /** 非 0：走公共池原子占位（50 人）；0 保持旧 ResolveOrCreate */
    uint64_t player_id = 0;
    std::string operation_id;
    uint32_t capacity = 0;  // 0=使用 public_map_capacity_
    uint32_t line_no = 0;
    std::string kind;  // 空 / LEGACY_POOL / LINE / DUNGEON
    uint32_t soft_cap = 0;
    uint32_t hard_cap = 0;
    uint32_t max_lines = 0;
    uint32_t min_lines = 0;
    uint32_t empty_close_delay = 0;
    std::string queue_token;
};

struct CreateDungeonInput {
    uint32_t realm_id = 0;
    uint64_t map_template_id = 0;
    uint64_t player_id = 0;
    std::vector<uint64_t> member_player_ids;
    std::string operation_id;
    std::string preferred_owner;
    uint32_t soft_cap = 0;
    uint32_t hard_cap = 0;
    uint32_t empty_close_delay = 0;
};

struct CreateDungeonResult {
    bool ok = false;
    std::string message;
    std::string error_code;
    PlacementRecord placement;
    std::vector<uint64_t> member_player_ids;
    bool idempotent_hit = false;
};

struct ResolveOrCreateResult {
    bool ok = false;
    std::string message;
    std::string error_code;
    PlacementRecord placement;
    uint32_t occupancy = 0;
    bool idempotent_hit = false;
};

struct MapLineInfo {
    uint32_t line_no = 0;
    uint64_t map_instance_id = 0;
    uint32_t occupancy = 0;
    uint32_t soft_cap = 0;
    uint32_t hard_cap = 0;
    std::string state;
    std::string owner_logic_server_id;
};

struct MapDungeonInfo {
    uint64_t map_instance_id = 0;
    uint64_t map_template_id = 0;
    uint32_t occupancy = 0;
    uint32_t members_n = 0;
    uint32_t soft_cap = 0;
    uint32_t hard_cap = 0;
    std::string state;
    std::string owner_logic_server_id;
};

struct SwitchLineInput {
    uint32_t realm_id = 0;
    uint64_t map_template_id = 0;
    uint64_t player_id = 0;
    uint32_t line_no = 0;
    std::string operation_id;
    uint32_t soft_cap = 0;
    uint32_t hard_cap = 0;
};

struct EnqueueMapInput {
    uint32_t realm_id = 0;
    uint64_t map_template_id = 0;
    uint64_t player_id = 0;
    uint32_t line_no = 0;
    std::string queue_token;
    uint32_t hard_cap = 0;
};

struct EnqueueMapResult {
    bool ok = false;
    std::string message;
    std::string error_code;
    std::string queue_token;
    uint32_t queue_position = 0;
    uint32_t queue_length = 0;
    uint32_t line_no = 0;
    bool ready = false;
};

class PlacementStore {
public:
    static PlacementStore &Instance();

    /** 与 SessionStore 共用 RedisPool；在 Session 启动时 Init。 */
    bool InitFromSessionPrefix(const std::string &key_prefix, int default_lease_sec = 30);
    bool Available() const { return available_; }

    /** 配置可承载 Placement 的 Logic Owner 列表；空则创建实例失败。 */
    void SetLogicOwners(std::vector<std::string> owners, bool publish_snapshot = true);

    /**
     * 解析或创建 Placement；player_id!=0 时走原子占位（幂等 operation_id）。
     * kind 空/LEGACY_POOL/LINE/DUNGEON 决定池与线策略。
     */
    bool ResolveOrCreate(const ResolveOrCreateInput &in, ResolveOrCreateResult *out);
    /** 公共池/指定实例原子占位；与 ResolveOrCreate(player_id!=0, LEGACY) 相同。 */
    bool ReservePublicSlot(const ResolveOrCreateInput &in, ResolveOrCreateResult *out);
    /** LINE 场景：按 line_no/soft_cap 占线；满员可配合 EnqueueMap。 */
    bool ReserveLine(const ResolveOrCreateInput &in, ResolveOrCreateResult *out);
    /** 已有副本实例上的进入占位（成员校验在 CreateDungeon 侧）。 */
    bool ReserveDungeonEnter(const ResolveOrCreateInput &in, ResolveOrCreateResult *out);
    /** 创建 DUNGEON 实例并登记成员；同 operation_id 幂等命中。 */
    bool CreateDungeon(const CreateDungeonInput &in, CreateDungeonResult *out);
    /** 列出某模板下各 LINE 的占用/状态（选线 UI）。 */
    bool ListLines(uint32_t realm_id, uint64_t map_template_id, std::vector<MapLineInfo> *out);
    /** 列出某模板下活跃 DUNGEON 摘要。 */
    bool ListDungeons(uint32_t realm_id, uint64_t map_template_id, std::vector<MapDungeonInfo> *out);
    /** 占用/线数变化钩子（进图、切线、离图、空线关闭）。测试可不设。 */
    using LineStatusHook = void (*)(uint32_t realm_id, uint64_t map_template_id);
    void SetLineStatusHook(LineStatusHook hook);
    /** 切线：释放旧线占位并 Reserve 目标 line_no（幂等）。 */
    bool SwitchLine(const SwitchLineInput &in, ResolveOrCreateResult *out);
    /** 满员排队；返回 queue_token/position，ready 时表示已可进。 */
    bool EnqueueMap(const EnqueueMapInput &in, EnqueueMapResult *out);
    /** 将实例标为 Draining 并拒绝新进；reason 写审计。 */
    bool Drain(uint64_t map_instance_id, const std::string &reason, PlacementRecord *out,
               std::string *err);
    /** 当前 map:occ 上的 player_id 列表。 */
    bool ListOccupants(uint64_t map_instance_id, std::vector<uint64_t> *out);
    /** 读 map:pres；无占位返回 false。 */
    bool GetPlayerPresence(uint64_t player_id, uint64_t *map_instance_id);
    /** 空线/空本超时关闭；now_unix=0 用当前时间。返回关闭的 instance id。 */
    bool CloseIdleInstances(int64_t now_unix, size_t limit, std::vector<uint64_t> *closed);
    /** 进图确认：把预约占位转为正式 occupancy（防只预约不进）。 */
    bool ConfirmSlot(uint64_t player_id, uint64_t map_instance_id);
    /** 离图/下线：释放该玩家全部地图占位（occ+pres）。 */
    bool ReleaseByPlayer(uint64_t player_id);
    /**
     * 断线宽限：从 map:occ 摘除以释放 softcap（幽灵人数），保留 map:pres。
     * 重连成功后调用 ResumeParked 加回 occ。
     */
    bool ParkForGrace(uint64_t player_id);
    bool ResumeParked(uint64_t player_id);
    /**
     * 扫 map:pres:*：keep_reservation(pid)==false 则 ReleaseByPlayer。
     * 用于下线/重启后 Session 已没、占位还在的幽灵 occupancy。
     */
    size_t ReclaimStaleReservations(const std::function<bool(uint64_t player_id)> &keep_reservation,
                                    size_t limit = 64);
    /**
     * 扫 map:occ:*：keep_online(pid)==false 则 SREM，并把 map:pres 标为 parked。
     * 用于 DISCONNECTED/缺失 Session 仍占 softcap 的幽灵人数。
     */
    size_t ReconcileGhostOccupancy(const std::function<bool(uint64_t player_id)> &keep_online,
                                   size_t limit = 128);
    /** 当前 map:occ 基数（SCARD）。 */
    uint32_t Occupancy(uint64_t map_instance_id);
    /** 粗算全部 map:occ:* 人数（metrics / 幽灵对账用，有扫描上限）。 */
    int64_t SumAllOccupancy(size_t max_keys = 64);
    /** 公共池默认 soft/hard 容量（Resolve 时 capacity=0 使用）。 */
    void SetPublicMapCapacity(uint32_t n);
    uint32_t public_map_capacity() const { return public_capacity_; }

    /** 读权威 PlacementRecord；不存在返回 false。 */
    bool Get(uint64_t map_instance_id, PlacementRecord *out);
    /**
     * Owner 迁移：CAS expect_epoch，换 new_owner 并抬升 epoch。
     * idempotency_key 防重试双切。
     */
    bool Migrate(uint64_t map_instance_id, const std::string &new_owner, uint64_t expect_epoch,
                 const std::string &idempotency_key, PlacementRecord *out, std::string *err);
    /** 标 Recovering，供后台选新 Owner 接管。 */
    bool MarkRecovering(uint64_t map_instance_id, const std::string &reason, PlacementRecord *out);
    /** Owner 续租：延长 lease_until；epoch/owner 不匹配失败。 */
    bool Heartbeat(uint64_t map_instance_id, const std::string &owner, uint64_t epoch,
                   uint32_t lease_sec, int64_t *lease_until_out);
    /** READY 且 lease 已过期 → RECOVERING；lease 仍有效返回 false */
    bool ExpireLeaseToRecovering(uint64_t map_instance_id, PlacementRecord *out, std::string *err);
    int default_lease_sec() const { return default_lease_sec_; }

    /** SCAN 发现 READY+lease 过期 或 RECOVERING 的 map_instance_id（供后台恢复） */
    bool ScanRecoveryCandidates(std::string *cursor, size_t count,
                                std::vector<uint64_t> *expired_ready,
                                std::vector<uint64_t> *recovering);
    /** 选择健康 Owner（可排除旧 Owner） */
    std::string PickHealthyOwner(const std::string &exclude) const;
    /** 审计事件追加到 Redis list（旧/新 Owner、epoch、原因） */
    void AppendAudit(uint64_t map_instance_id, const std::string &event,
                     const std::string &old_owner, const std::string &new_owner, uint64_t old_epoch,
                     uint64_t new_epoch, const std::string &reason);

    static std::string StateToString(PlacementState s);
    static PlacementState StateFromString(const std::string &s);

private:
    PlacementStore() = default;
    std::string InstKey(uint64_t id) const;
    std::string TplKey(uint32_t realm, uint64_t tpl) const;
    std::string IdGenKey() const;
    /** 健康列表非空；无显式 preferred 时返回空串，交给 Lua 按 idgen 取模。 */
    std::string CreateOwnerHint(const std::string &preferred) const;
    bool HasHealthyOwners() const;
    std::vector<std::string> OwnersForPick() const;
    /** 当前健康 Owner 列表 CSV，供 ResolveOrCreate Lua 判断软续租 */
    std::string HealthyOwnersCsv() const;

    std::string LinesKey(uint32_t realm, uint64_t tpl) const;
    std::string DungeonsKey(uint32_t realm, uint64_t tpl) const;
    std::string LineKey(uint32_t realm, uint64_t tpl, uint32_t line_no) const;
    std::string PoolKey(uint32_t realm, uint64_t tpl) const;
    std::string OccKey(uint64_t map_instance_id) const;
    std::string PresKey(uint64_t player_id) const;
    std::string OpKey(uint64_t player_id, const std::string &operation_id) const;
    std::string MembersKey(uint64_t map_instance_id) const;
    std::string IdleKey() const;
    std::string QueueKey(uint32_t realm, uint64_t tpl, uint32_t line_no) const;
    void FireLineStatus(uint32_t realm_id, uint64_t map_template_id);
    bool HasLineStatusHook() const;

    bool available_ = false;
    int default_lease_sec_ = 30;
    uint32_t public_capacity_ = 50;
    std::string key_prefix_ = "lunaserver:dev:";
    mutable std::mutex cfg_mu_;
    std::vector<std::string> owners_{"gl-0"};
    mutable size_t rr_ = 0;
    LineStatusHook line_status_hook_ = nullptr;
};
