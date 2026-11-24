#pragma once

/**
 * @file ClientSeqGate.h
 * @brief 客户端移动/操作序号窗口门控：幂等重放或拒绝陈旧 seq
 *
 * client_seq==0 视为无序号（兼容旧客户端）直接 Execute；等于 last 则 Idempotent
 * 并回放 cached_frame；小于 last 则 Reject（ERR_STALE_SEQ）。供 MapRuntime Move 等使用。
 */

#include <cstdint>
#include <string>

/** 序号窗口判定结果。 */
enum class ClientSeqDecision { Execute, Idempotent, Reject };

/**
 * 根据上次已接受的 client_seq 判定本次请求。
 * @param last_client_seq 连接/玩家上已接受的最大序号；0 表示尚无
 * @param client_seq 本帧携带序号
 * @param cached_frame 与 last 对应的已编码响应，供幂等重放
 * @param out_cached 幂等时填入 cached_frame
 * @param err Reject 时写入公开错误码
 */
inline ClientSeqDecision EvaluateClientSeq(uint64_t last_client_seq, uint64_t client_seq,
                                           const std::string &cached_frame,
                                           std::string *out_cached, std::string *err) {
    if (client_seq == 0)
        return ClientSeqDecision::Execute;
    if (client_seq == last_client_seq && last_client_seq != 0) {
        if (out_cached)
            *out_cached = cached_frame;
        return ClientSeqDecision::Idempotent;
    }
    if (last_client_seq != 0 && client_seq < last_client_seq) {
        if (err)
            *err = "ERR_STALE_SEQ";
        return ClientSeqDecision::Reject;
    }
    return ClientSeqDecision::Execute;
}
