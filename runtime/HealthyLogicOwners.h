#pragma once

/**
 * @file HealthyLogicOwners.h
 * @brief 刷新可分配的健康 GameLogic Owner 列表（三态语义）
 *
 * RPC 前与周期刷新共用：Registry 未就绪保留静态/末次快照；Discover 失败保留并记指标；
 * 成功非空则原子替换；成功零实例时，若曾有过非空健康集则清空（新登录 fail-closed），
 * 否则保留启动静态快照。
 *
 * 协作：HealthyLogicSnapshotStore、IServiceRegistry、Placement 选 Owner。
 */

#include <cstddef>

/**
 * GameLogic 健康快照刷新状态。
 * - kNotReady：Registry 未就绪
 * - kDiscoverFailed：Discover 调用失败
 * - kApplied：Discover 成功并已按规则更新快照（含空列表清空）
 */
enum class HealthyLogicRefreshStatus {
    kNotReady = 0,
    kDiscoverFailed = 1,
    kApplied = 2,
};

struct HealthyLogicRefreshResult {
    HealthyLogicRefreshStatus status = HealthyLogicRefreshStatus::kNotReady;
    /** Discover 成功时的实例数量（可为空）。 */
    std::size_t instance_count = 0;
};

/**
 * 执行一次健康 Logic 列表刷新。
 * @param update_static_addrs true 时同步刷新启动静态地址表（少用）
 */
HealthyLogicRefreshResult RefreshHealthyLogicOwners(bool update_static_addrs = false);
