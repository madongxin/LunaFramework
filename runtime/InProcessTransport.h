#pragma once

/**
 * @file InProcessTransport.h
 * @brief 进程内投递：Gateway IO 线程 → 玩家串行队列 → HandleFrame → ReplySink
 *
 * 单进程部署下的默认 ITransport：按 player_id 投递到 PlayerSerialQueue，
 * 在队列线程调用 GameService::HandleFrame，再经 sink 回写。
 * EnsureStarted 须在开始 Post 前调用以启动分片队列。
 *
 * 协作：GameRequestTransport 未注入其它后端时回落本实现；多进程时由 BrpcTransport 替代。
 */

#include "ITransport.h"

/** 单进程内投递：串行队列上调用 GameService::HandleFrame。 */
class InProcessTransport : public ITransport {
public:
    static InProcessTransport &Instance();

    /**
     * 确保 PlayerSerialQueue 已启动。
     * @param shard_count 分片数；0 表示使用队列默认配置
     */
    void EnsureStarted(int shard_count = 0);

    /**
     * 按 handle.player_id 投递到串行队列；队列线程内 HandleFrame 后 ReplySink::Reply。
     * 不得在 Gateway reactor 线程同步执行业务。
     */
    void PostPlayerRequest(const SessionHandle &handle,
                           std::string request_payload,
                           std::shared_ptr<ReplySink> sink) override;
};
