/**
 * @file GatewayAuthFlow.cpp
 * @brief GatewayAuthFlow 实现：连接槽位、flow_gen 与 in-flight 互斥
 *
 * 全部公开方法经 mu_ 串行化；OnDisconnected 保留 slot 直至匹配的 End，
 * 避免异步回调窗口内 AcceptCallback 误判。
 */

#include "GatewayAuthFlow.h"

GatewayAuthFlow &GatewayAuthFlow::Instance() {
    static GatewayAuthFlow g;
    return g;
}

/** 连接建立：alive=true，清空 in_flight，flow_gen 从 0 起算。 */
void GatewayAuthFlow::OnConnected(uint64_t connection_id) {
    std::lock_guard<std::mutex> lk(mu_);
    Slot &s = slots_[connection_id];
    s.alive = true;
    s.in_flight = false;
    // 保留 flow_gen 递增空间；新连接从 0 起
    s.flow_gen = 0;
}

/** 连接断开：标记非 alive；slot 可保留到 End 再 erase。 */
void GatewayAuthFlow::OnDisconnected(uint64_t connection_id) {
    std::lock_guard<std::mutex> lk(mu_);
    auto it = slots_.find(connection_id);
    if (it == slots_.end())
        return;
    it->second.alive = false;
    it->second.in_flight = false;
    // 保留 slot 直至回调 End，避免 Accept 误判；也可 erase
}

/** 开编排：要求 alive 且无 in-flight；成功则 ++flow_gen。 */
bool GatewayAuthFlow::TryBegin(uint64_t connection_id, uint64_t *flow_gen_out) {
    std::lock_guard<std::mutex> lk(mu_);
    Slot &s = slots_[connection_id];
    if (!s.alive)
        return false;
    if (s.in_flight)
        return false;
    s.in_flight = true;
    ++s.flow_gen;
    if (flow_gen_out)
        *flow_gen_out = s.flow_gen;
    return true;
}

/** 结束编排：代次匹配才清 in_flight；已死则擦除 slot。 */
void GatewayAuthFlow::End(uint64_t connection_id, uint64_t flow_gen) {
    std::lock_guard<std::mutex> lk(mu_);
    auto it = slots_.find(connection_id);
    if (it == slots_.end())
        return;
    if (it->second.flow_gen != flow_gen)
        return;
    it->second.in_flight = false;
    if (!it->second.alive)
        slots_.erase(it);
}

/** 回调校验：alive 且 flow_gen 一致才可落地。 */
bool GatewayAuthFlow::AcceptCallback(uint64_t connection_id, uint64_t flow_gen) const {
    std::lock_guard<std::mutex> lk(mu_);
    auto it = slots_.find(connection_id);
    if (it == slots_.end())
        return false;
    return it->second.alive && it->second.flow_gen == flow_gen;
}

bool GatewayAuthFlow::Alive(uint64_t connection_id) const {
    std::lock_guard<std::mutex> lk(mu_);
    auto it = slots_.find(connection_id);
    return it != slots_.end() && it->second.alive;
}
