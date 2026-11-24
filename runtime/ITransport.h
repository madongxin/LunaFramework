#pragma once

/**
 * @file ITransport.h
 * @brief 玩家请求投递抽象：Gateway → 业务处理 → ReplySink
 *
 * 阶段 1 仅 InProcessTransport（同进程串行队列）；阶段 2+ 可换 BrpcTransport
 * 跨进程投递。调用方经 GameRequestTransport::Get() 取得当前后端，禁止直接依赖具体实现。
 *
 * 约束：PostPlayerRequest 不得在调用线程同步跑重业务；实现侧负责投递到正确执行上下文。
 */

#include "SessionHandle.h"
#include "ReplySink.h"

#include <memory>
#include <string>

/**
 * 投递玩家请求的传输抽象。
 * sink 在业务完成后回写响应（可能跨线程，实现须保证线程安全语义）。
 */
class ITransport {
public:
    virtual ~ITransport() = default;
    /**
     * 投递一条已绑定会话的玩家请求。
     * @param handle 含 player_id / fence / 地图路由等；实现可据此选 shard 或 Logic 实例
     * @param request_payload 已序列化的 GameRequest（或帧载荷）
     * @param sink 异步回复出口；实现不得假设 sink 在 Post 返回后仍仅由本线程持有
     */
    virtual void PostPlayerRequest(const SessionHandle &handle,
                                   std::string request_payload,
                                   std::shared_ptr<ReplySink> sink) = 0;
};
