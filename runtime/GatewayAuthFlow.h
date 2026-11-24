#pragma once

/**
 * @file GatewayAuthFlow.h
 * @brief Gateway 侧 Login/Reconnect 编排代次与 in-flight 互斥
 *
 * 每条 TCP 连接同一时刻只允许一个 Auth→Session→Bind 编排在飞。回调落地前必须
 * AcceptCallback(connection_id, flow_gen)：连接仍存活且代次匹配，防止断线/重入后
 * 迟到回调写脏绑定。与 LoginAdmission（进程级预算）正交：本类管单连接互斥与代次。
 *
 * 协作：GatewayLoginOrchestrator 在开编排时 TryBegin，结束/失败时 End；
 * OnConnected/OnDisconnected 由 Gateway 连接生命周期驱动。
 */

#include <cstdint>
#include <mutex>
#include <unordered_map>

/**
 * 每连接 Login/Reconnect 流程代次守卫。
 * - 同一连接同时只允许一个 in-flight 编排
 * - 回调时校验 connection 仍存活且 flow_gen 匹配，防止迟到回调写脏绑定
 */
class GatewayAuthFlow {
public:
    /** 进程内单例；Gateway 各 reactor 线程经 mutex 共享。 */
    static GatewayAuthFlow &Instance();

    /** 连接建立：标记 alive，清空 in_flight，flow_gen 重置为 0。 */
    void OnConnected(uint64_t connection_id);
    /**
     * 连接断开：标记非 alive 并清 in_flight。
     * slot 可保留至 End，避免迟到 AcceptCallback 误判为「无 slot」。
     */
    void OnDisconnected(uint64_t connection_id);

    /**
     * 开始编排；若连接不存在、已死或已有 in-flight 返回 false。
     * 成功时递增 flow_gen 并经 flow_gen_out 回传（可为 nullptr）。
     */
    bool TryBegin(uint64_t connection_id, uint64_t *flow_gen_out);
    /**
     * 结束编排：仅当 flow_gen 仍匹配时清 in_flight；
     * 若连接已死则擦除 slot。代次不匹配时为迟到 End，忽略。
     */
    void End(uint64_t connection_id, uint64_t flow_gen);

    /**
     * 异步回调落地前校验：连接仍 alive 且 generation 与发起时一致。
     * 不匹配则丢弃回调，禁止写会话/玩家绑定。
     */
    bool AcceptCallback(uint64_t connection_id, uint64_t flow_gen) const;
    /** 连接是否仍登记为存活（不考虑 in_flight）。 */
    bool Alive(uint64_t connection_id) const;

private:
    struct Slot {
        bool alive = false;
        bool in_flight = false;
        uint64_t flow_gen = 0;
    };
    GatewayAuthFlow() = default;
    mutable std::mutex mu_;
    std::unordered_map<uint64_t, Slot> slots_;
};
