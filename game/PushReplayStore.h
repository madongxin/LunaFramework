#pragma once

/**
 * @file PushReplayStore.h
 * @brief 跨 Gateway 共享的可靠 Push 回放存储（Redis）
 *
 * Key 必须含 session_id：新 Session 不继承旧 Session 私有消息。
 * 与 SessionStore 共用 key_prefix；Append 分配单调 server_seq，
 * 重连按 last_acked_seq ReplayAfter；空洞或超窗则 need_snapshot。
 * Ack/CommitSnapshot 裁剪 list，防止无限膨胀。
 */

#include "PushReplayCache.h"

#include <cstdint>
#include <string>
#include <vector>

class PushReplayStore {
public:
    static PushReplayStore &Instance();

    /**
     * 用 Session 的 key_prefix 初始化；cap 为每 session 回放条数上限，ttl_sec 为 key TTL。
     * Redis 不可用时 Available()==false，可靠 Push 应降级或 fail-closed。
     */
    bool InitFromSessionPrefix(const std::string &key_prefix, size_t cap = 1024,
                               int ttl_sec = 7200);
    bool Available() const { return available_; }

    /**
     * 原子分配 server_seq 并追加可靠消息。
     * @return 成功返回 seq>0；不可用或编码失败返回 0
     */
    uint64_t AppendReliable(uint64_t player_id, const std::string &session_id,
                            const std::string &message_type, const std::string &payload);

    /**
     * 仅 INCR 预留 seq（先编码带 baseline 的 payload 再 AppendReserved）。
     * 避免「先写入再改 payload」造成客户端看到的序号与内容不一致。
     */
    uint64_t ReserveSeq(uint64_t player_id, const std::string &session_id);

    /**
     * 将已预留的 seq 与完整 payload 写入 replay list。
     * 若当前 seq 计数与 reserved 不一致返回 false（产生 gap，由重连检测）。
     */
    bool AppendReserved(uint64_t player_id, const std::string &session_id, uint64_t reserved_seq,
                        const std::string &message_type, const std::string &payload);

    /**
     * 回放 last_acked_seq 之后的可靠消息到 *out。
     * 缺口过大或元数据损坏时 *need_snapshot=true，调用方应下发全量快照。
     */
    bool ReplayAfter(uint64_t player_id, const std::string &session_id, uint64_t last_acked_seq,
                     std::vector<PushReplayEntry> *out, bool *need_snapshot);

    enum class AckStatus {
        Ok = 0,
        Duplicate = 1,
        Ahead = 2,
        Stale = 3,
        Unavailable = 4,
        Invalid = 5,
        Gap = 6,  // ACK 跳过空洞 / 指向未写入 seq → NEED_SNAPSHOT
    };
    struct AckResult {
        AckStatus status = AckStatus::Invalid;
        std::string error_code;
        uint64_t trimmed_to_seq = 0;
        bool ok() const {
            return status == AckStatus::Ok || status == AckStatus::Duplicate;
        }
    };
    /**
     * 原子校验 current_seq / last_ack 后裁剪已确认条目。
     * Redis 不可用返回 Unavailable（调用方不得当成成功）。
     */
    AckResult Ack(uint64_t player_id, const std::string &session_id, uint64_t ack_seq);

    /**
     * 全量快照已成功交给客户端后调用。
     * 原子地把 lastAck、snapBaseline 抬到 max(原值, baseline)，并丢掉 seq≤该水位的条目。
     * baseline 超过当前 seq 时夹到当前 seq，不会把光标推到尚未分配的序号。
     */
    bool CommitSnapshot(uint64_t player_id, const std::string &session_id, uint64_t baseline);

    /** 当前已分配的最大 server_seq（无 key 为 0）。 */
    uint64_t CurrentSeq(uint64_t player_id, const std::string &session_id);

    /** 顶号/新 Session：删除旧 session 的 seq/list/meta，避免串线回放。 */
    bool InvalidateSession(uint64_t player_id, const std::string &session_id);

private:
    PushReplayStore() = default;
    std::string SeqKey(uint64_t player_id, const std::string &session_id) const;
    std::string ListKey(uint64_t player_id, const std::string &session_id) const;
    std::string MetaKey(uint64_t player_id, const std::string &session_id) const;

    bool available_ = false;
    std::string key_prefix_ = "lunaserver:dev:";
    size_t cap_ = 1024;
    int ttl_sec_ = 7200;
};
