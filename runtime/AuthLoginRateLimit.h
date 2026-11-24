#pragma once

/**
 * @file AuthLoginRateLimit.h
 * @brief 多 Auth 实例共享的登录失败限流（Redis；不可用时进程内降级）
 *
 * 按 player_id 在滑动窗口内累计失败次数，超过 max_fails 则 IsLimited 拒绝继续尝试。
 * 成功登录应 Clear；失败路径 RecordFail。Redis 键前缀可 Configure 定制，便于环境隔离。
 *
 * 协作：AuthServiceImpl Login 失败/成功路径；与 GatewayConnGuard 的帧/命令限流分层。
 */

#include <cstdint>
#include <string>

/** 跨 Auth 实例共享的登录失败限流器（单例）。 */
class AuthLoginRateLimit {
public:
    static AuthLoginRateLimit &Instance();

    /**
     * 配置 Redis 键前缀与窗口参数。
     * @param key_prefix Redis key 前缀（如 lunaserver:prod:）
     * @param window_sec 失败计数窗口秒数（默认 60）
     * @param max_fails 窗口内允许的最大失败次数（默认 10）
     */
    void Configure(const std::string &key_prefix, int window_sec = 60, int max_fails = 10);
    /**
     * 该 player 当前是否处于限流中。
     * Redis 可用时读共享计数；不可用时读进程内降级表。
     */
    bool IsLimited(uint64_t player_id);
    /** 记录一次登录失败（窗口内累加）。 */
    void RecordFail(uint64_t player_id);
    /** 登录成功或管理重置时清除该 player 的失败计数。 */
    void Clear(uint64_t player_id);

private:
    AuthLoginRateLimit() = default;
    std::string key_prefix_ = "lunaserver:dev:";
    int window_sec_ = 60;
    int max_fails_ = 10;
};
