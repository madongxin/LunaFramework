#pragma once

/**
 * @file MapInstanceRegistry.h
 * @brief GameLogic 本地地图实例：Claim + epoch + owner lease 写栅栏
 *
 * 记录本进程声称拥有的 map_instance（模板、epoch、lease_until、在场玩家）。
 * CheckWrite/AcceptWrite 在移动与 AOI 写路径前置：未 Claim、epoch 过期或 lease
 * 失效则拒写。Formal 分布式下 SetRequireLease(true) 时 lease_until==0 视为缺失。
 *
 * 协作：MapLeaseKeeper 周期 HeartbeatOwner 续租；MapPlacement/PlacementStore 为权威放置；
 * 迁移后旧 Owner 须 Release，旧 epoch 写必须被拒绝。
 */

#include <cstdint>
#include <mutex>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

/** 本进程持有的一张地图实例元数据。 */
struct LocalMapInstance {
    uint64_t map_template_id = 0;
    uint64_t map_instance_id = 0;
    uint64_t owner_epoch = 0;
    /** Unix 秒；0 = 未启用本地 lease（仅测试/无 Placement）。 */
    int64_t lease_until_unix = 0;
    std::unordered_set<uint64_t> players;
};

/** 写栅栏结果：Ok 以外均应拒绝本图权威写。 */
enum class MapWriteFence {
    Ok = 0,
    NotClaimed,
    StaleEpoch,
    LeaseExpired,
    LeaseMissing,  // formal：lease_until==0 / 未启用
};

class MapInstanceRegistry {
public:
    static MapInstanceRegistry &Instance();

    /** 设置本 GameLogic 实例 ID（日志/对账用）。 */
    void SetLocalInstanceId(std::string id);
    const std::string &local_instance_id() const { return local_id_; }

    /** Formal 分布式：lease_until==0 视为缺失并拒写。 */
    void SetRequireLease(bool on);
    bool require_lease() const { return require_lease_; }

    /**
     * Claim / 升 epoch；同 epoch 幂等；更低 epoch 拒绝。
     * lease_until_unix 为 0 时表示尚未启用租约（非正式/测试）。
     */
    bool Claim(uint64_t map_instance_id, uint64_t map_template_id, uint64_t owner_epoch,
               int64_t lease_until_unix = 0);

    /** 更新本地记录的 lease 截止时间（续租成功后）。 */
    void SetLeaseUntil(uint64_t map_instance_id, int64_t lease_until_unix);
    int64_t LeaseUntil(uint64_t map_instance_id) const;

    /** 细粒度写栅栏诊断；AcceptWrite 为其 Ok 判定封装。 */
    MapWriteFence CheckWrite(uint64_t map_instance_id, uint64_t owner_epoch) const;
    /** 请求 epoch 必须等于本地 Claim，且本地 lease 未过期（lease=0 且未 require 时跳过）。 */
    bool AcceptWrite(uint64_t map_instance_id, uint64_t owner_epoch) const;

    bool Has(uint64_t map_instance_id) const;
    uint64_t Epoch(uint64_t map_instance_id) const;
    uint64_t TemplateId(uint64_t map_instance_id) const;

    bool AddPlayer(uint64_t map_instance_id, uint64_t player_id);
    bool RemovePlayer(uint64_t map_instance_id, uint64_t player_id);
    /** 断线 Unbind：从所有本地地图移除玩家，并释放空地图 Claim。 */
    size_t RemovePlayerFromAll(uint64_t player_id);
    bool PlayerOnMap(uint64_t map_instance_id, uint64_t player_id) const;
    uint32_t PlayerCount(uint64_t map_instance_id) const;

    /** 放弃本地权威（迁移后旧 Owner / lease 失效）。 */
    void Release(uint64_t map_instance_id);

    /** 释放所有本地 lease 已过期的地图；返回释放数量。 */
    size_t ReleaseExpired();

    /** (map_instance_id, owner_epoch) 列表，供 MapLeaseKeeper 续租。 */
    std::vector<std::pair<uint64_t, uint64_t>> ListOwned() const;

    void ClearForTest();

private:
    MapInstanceRegistry() = default;
    static int64_t NowUnixSec();

    mutable std::mutex mu_;
    std::string local_id_ = "gl-local";
    bool require_lease_ = false;
    std::unordered_map<uint64_t, LocalMapInstance> maps_;
};
