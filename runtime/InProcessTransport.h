#pragma once


/**
 * InProcessTransport.h — 进程内投递：Gateway IO 线程 → 玩家串行队列 → 回投 EventLoop。
 * 路径: runtime/InProcessTransport.h
 */
#include "ITransport.h"

/** 单进程内投递：串行队列上调用 GameService::HandleFrame */
class InProcessTransport : public ITransport {
public:
    static InProcessTransport &Instance();

    void EnsureStarted(int shard_count = 0);

    void PostPlayerRequest(const SessionHandle &handle,
                           std::string request_payload,
                           std::shared_ptr<ReplySink> sink) override;
};
