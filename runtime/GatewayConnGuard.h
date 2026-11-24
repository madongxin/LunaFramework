#pragma once

/**
 * @file GatewayConnGuard.h
 * @brief Gateway 连接级 Hello/心跳/限流/空闲守卫（进程内，无外部存储）
 *
 * 维护每连接的握手状态、帧/鉴权/聊天/查名速率、空闲计时与对端 IP。
 * 禁止在此访问 Redis/MySQL——限流与超时必须是纯内存、可在 reactor 线程调用。
 * IP 级建连速率与帧级速率配合 ProtocolHandshake / OpsMetrics 使用。
 *
 * 协作：Gateway 收包路径 NoteActivity + Check*；空闲踢人用 IdleExpired；
 * 读暂停期间 AddIdleCredit 避免误杀。
 */

#include <cstdint>
#include <string>

/** 连接级 Hello/心跳/限流状态。禁止在此访问 Redis/MySQL。 */
class GatewayConnGuard {
public:
    static GatewayConnGuard &Instance();

    /** 登记新连接：记录 fd 对应 peer IP 与接入时刻。 */
    void OnConnected(uint64_t conn_id, int fd);
    /** 连接关闭：擦除连接状态（不影响其它连接的 IP 桶）。 */
    void OnDisconnected(uint64_t conn_id);
    /** 收到有效帧时刷新活动时间与帧字节窗口（供限流/空闲判定）。 */
    void NoteActivity(uint64_t conn_id, size_t frame_bytes);

    /** 该连接是否已通过 ClientHello（未登记视为 false）。 */
    bool HelloOk(uint64_t conn_id) const;
    /** 标记 Hello 完成；仅应在协议校验成功后调用。 */
    void SetHelloOk(uint64_t conn_id, bool ok);

    /**
     * 以下 Check* 超限返回非空错误码字符串；空串表示通过。
     * 调用方据此回错误帧或触发关连，不在本类内直接关 fd。
     */
    /** 按 peer IP 的建连速率；fd 用于解析 IP。 */
    std::string CheckConnectRate(int fd);
    /** 帧速率/字节速率（滑动窗口）。 */
    std::string CheckFrameRate(uint64_t conn_id, size_t frame_bytes);
    /** 心跳速率；bound=true 表示已绑定玩家（阈值可不同）。 */
    std::string CheckHeartbeatRate(uint64_t conn_id, bool bound);
    /** Login/Reconnect 等鉴权命令速率。 */
    std::string CheckAuthCommandRate(uint64_t conn_id);
    /** 聊天发送速率。 */
    std::string CheckChatRate(uint64_t conn_id);
    /** 按名查询速率。 */
    std::string CheckNameQueryRate(uint64_t conn_id);

    /** 自上次活动起是否超过 idle_ms（已扣减 AddIdleCredit）。 */
    bool IdleExpired(uint64_t conn_id, uint32_t idle_ms) const;
    /** 从接入到现在的毫秒；连接已不在表里时返回 0。 */
    int64_t ConnectedAgeMs(uint64_t conn_id) const;
    /** 限流错误回复每条连接每秒最多放行一次，避免错误风暴。 */
    bool AllowRateError(uint64_t conn_id);
    /** 连续处于限流状态达到 ms 则调用方应关连接。 */
    bool RateLimitedTooLong(uint64_t conn_id, int64_t ms);
    /** 服务端暂停读的时间不计入空闲超时（累加 credit）。 */
    void AddIdleCredit(uint64_t conn_id, int64_t paused_ms);
    /** 连接对端 IP 字符串；未知或已断开时可能为 "unknown"/空。 */
    std::string PeerIp(uint64_t conn_id) const;

private:
    GatewayConnGuard() = default;
};
