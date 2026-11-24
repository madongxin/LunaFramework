#pragma once


/**
 * @file BrpcTransport.h
 * @brief 跨进程 ITransport：经 brpc 将玩家请求投递到 GameLogic
 *
 * 与 InProcessTransport 二选一由 GameRequestTransport 注入；须带 SessionHandle。
 */
#include "ITransport.h"

#include <memory>
#include <string>
#include <vector>

namespace brpc {
class Channel;
}

/** Gateway → GameLogic / World 的 brpc 异步转发 */
class BrpcTransport : public ITransport {
public:
    static BrpcTransport &Instance();

    bool EnsureStarted(const std::vector<std::string> &logic_addrs, int timeout_ms = 3000);
    bool EnsureStarted(const std::vector<std::string> &logic_addrs,
                       const std::vector<std::string> &logic_instance_ids, int timeout_ms = 3000);
    bool EnsureStarted(const std::vector<std::string> &logic_addrs,
                       const std::vector<std::string> &logic_instance_ids,
                       const std::vector<std::string> &globalservice_addrs, int timeout_ms = 3000);

    void PostPlayerRequest(const SessionHandle &handle,
                           std::string request_payload,
                           std::shared_ptr<ReplySink> sink) override;

    bool globalservice_ready() const { return globalservice_channel_ != nullptr; }

private:
    bool started_ = false;
    std::unique_ptr<brpc::Channel> globalservice_channel_;
};
