#pragma once

/**
 * @file GatewayIdentity.h
 * @brief 本 Gateway 的 instance_id 解析、缓存与 Redis 声明
 *
 * 稳定 Gateway 实例身份：由配置/环境注入，禁止从 0.0.0.0:port 拼接（多机撞 ID）。
 * 解析优先级：显式 Set > LUNASERVER_INSTANCE_ID > gateway.cnf 的 gateway_instance_id。
 * Formal 模式下 Resolve 失败必须使进程启动失败；PushBatch 按 gateway_instance_id 定向单 Gateway。
 *
 * 协作：ClaimOrFail/RenewClaim 用 Redis SET NX + TTL 防双进程撞 ID；GameLogic Push 路由依赖本 id。
 */

#include <string>

/**
 * 稳定 Gateway 实例身份单例。
 * Resolve 成功后 id() 只读缓存；ClaimOrFail 可选在 Redis 上声明所有权。
 */
class GatewayIdentity {
public:
    static GatewayIdentity &Instance();

    /**
     * 按优先级解析并缓存 instance_id。
     * 解析结果为空时返回 false 且经 err 写出原因（正式模式调用方必须失败退出）。
     */
    bool Resolve(std::string *err);
    /** 测试或运维显式注入；覆盖此前 Resolve 结果。 */
    void Set(const std::string &id);

    const std::string &id() const { return id_; }
    /** 是否已成功 Resolve/Set（非空）。 */
    bool ready() const { return !id_.empty(); }

    /**
     * 可选：Redis SET NX 声明 instance_id，防止双进程撞 ID。
     * Redis 不可用时仅校验非空并返回 true（降级）；冲突时返回 false 并写 err。
     */
    bool ClaimOrFail(std::string *err);
    /**
     * 刷新 gw_claim TTL。
     * 应在 Gateway 主循环周期调用，避免 TTL 过期后被其它进程抢走导致误杀 ONLINE。
     */
    bool RenewClaim();

private:
    GatewayIdentity() = default;
    std::string ClaimKey() const;
    std::string id_;
};

/** 供无类上下文快速读取已缓存 id（未 Resolve 时为空串）。 */
inline const std::string &GatewayInstanceId() { return GatewayIdentity::Instance().id(); }
