/**
 * @file InProcessTransport.cpp
 * @brief InProcessTransport 实现：投递 PlayerSerialQueue 并在队列线程 HandleFrame
 *
 * PostPlayerRequest 捕获 payload/sink/player_id；失败仅打日志，成功则 sink->Reply。
 */

#include "InProcessTransport.h"

#include "GameService.h"
#include "Logging.h"
#include "PlayerSerialQueue.h"

InProcessTransport &InProcessTransport::Instance() {
    static InProcessTransport g;
    return g;
}

void InProcessTransport::EnsureStarted(int shard_count) {
    PlayerSerialQueue::Instance().Start(shard_count);
}

void InProcessTransport::PostPlayerRequest(const SessionHandle &handle,
                                           std::string request_payload,
                                           std::shared_ptr<ReplySink> sink) {
    const uint64_t player_id = handle.player_id;
    PlayerSerialQueue::Instance().Post(
        player_id,
        [payload = std::move(request_payload), sink, player_id]() mutable {
            std::string out;
            if (!gameproto::HandleFrame(payload, &out)) {
                LOG_ERROR << "InProcessTransport: HandleFrame failed player_id="
                          << player_id;
                return;
            }
            if (sink)
                sink->SendFrame(out);
        });
}
