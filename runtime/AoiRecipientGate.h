#pragma once

/**
 * @file AoiRecipientGate.h
 * @brief AOI 收件人门控：网关无会话时短时 Suppress，避免空推
 *
 * BindPlayer 成功后 Clear。不删除地图实体，也不等于 Logout。
 * Allowed 为 false 时上层跳过组批/发送。
 */

#include <cstdint>
#include <mutex>
#include <unordered_set>

/**
 * 网关已经找不到会话时，短时间内不再给这个收件人组 AOI。
 * BindPlayer 成功后清除。不删除地图实体，也不等于 Logout。
 */
class AoiRecipientGate {
public:
    static AoiRecipientGate &Instance();

    /** 是否允许向该玩家组 AOI；已 Suppress 返回 false。 */
    bool Allowed(uint64_t player_id) const;
    /** 网关找不到会话时调用：短时禁止再向该收件人推送。 */
    void Suppress(uint64_t player_id);
    /** BindPlayer 成功后清除 Suppress，恢复收件。 */
    void Clear(uint64_t player_id);
    /** 测试或进程重置：清空全部 Suppress。 */
    void ClearAll();

private:
    AoiRecipientGate() = default;
    mutable std::mutex mu_;
    std::unordered_set<uint64_t> suppressed_;
};
