#pragma once

/**
 * @file MapLeaseKeeper.h
 * @brief 周期续租本机拥有的地图 lease，防止被其它 Logic 抢走
 *
 * 对 MapInstanceRegistry::ListOwned 的每张图调用 Session HeartbeatOwner；
 * 续租失败或本地 lease 已过期 → Release（fail-closed），避免双主写。
 *
 * 协作：由 GameLogic EventLoop 周期 Tick；lease_sec 应小于 Placement 侧 TTL 并留心跳裕量。
 */

#include <cstdint>

/**
 * GameLogic 侧 Owner lease 续租器。
 * 续租失败或本地 lease 过期 → Release（fail-closed）。
 */
class MapLeaseKeeper {
public:
    static MapLeaseKeeper &Instance();

    /** 期望租约时长（秒）；Tick 内传给 HeartbeatOwner。 */
    void SetLeaseSec(uint32_t sec);
    uint32_t lease_sec() const { return lease_sec_; }

    /** 由 EventLoop 周期调用：续租全部本地 Claim；失败则 Release。 */
    void Tick();

private:
    MapLeaseKeeper() = default;
    uint32_t lease_sec_ = 30;
};
