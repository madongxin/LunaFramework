#pragma once

/**
 * @file HealthProbe.h
 * @brief 后台主动探测依赖健康并缓存快照
 *
 * Refresh(role) 调用 EvaluateHealthDeps 并更新 Current 快照（含连续成败计数）。
 * Fresh(max_age_ms) 供 readiness 判定探测是否过期。Gateway 另可 SetGatewayTcpListening
 * 反映 TCP accept 是否已起来。
 *
 * 协作：ServiceHealth::ReadinessJson；周期定时器驱动 Refresh。
 */

#include <cstdint>
#include <memory>
#include <string>

/** 一次探测的缓存结果。 */
struct HealthProbeSnapshot {
    bool ok = false;
    std::string detail = "uninitialized";
    int64_t updated_unix_ms = 0;
    int consecutive_failures = 0;
    int consecutive_successes = 0;
};

class HealthProbeStore {
public:
    static HealthProbeStore &Instance();

    /** 按角色跑依赖检查并原子替换快照。 */
    void Refresh(const std::string &role);
    /** 当前快照（shared_ptr，可无锁读）。 */
    std::shared_ptr<const HealthProbeSnapshot> Current() const;
    /** 快照是否在 max_age_ms 内更新过（默认 15s）。 */
    bool Fresh(int max_age_ms = 15000) const;

    /** Gateway：TCP 监听是否已 Start（与 deps 并列影响 ready）。 */
    static void SetGatewayTcpListening(bool v);
    static bool GatewayTcpListening();

private:
    HealthProbeStore() = default;
    std::shared_ptr<const HealthProbeSnapshot> snap_{std::make_shared<HealthProbeSnapshot>()};
};
