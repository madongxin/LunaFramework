#pragma once

/**
 * @file PlayerItemPersistQueue.h
 * @brief 道具变更内存队列：定时/登出批量刷入 PlayerItemStore
 */

#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_set>

class EventLoop;

/** 待持久化的一条道具变更。snapshot 为真时 count 是该道具的剩余总量，不是增量。 */
struct PendingPlayerItem {
    uint64_t player_id = 0;
    uint64_t item_id = 0;
    uint32_t count = 0;
    int64_t expire_time_sec = 0;
    std::string extra_data;
    bool snapshot = false;
};

class PlayerItemPersistQueue {
public:
    static PlayerItemPersistQueue &Instance();

    /** 注册到主 EventLoop 定时器（与 MetricsDbWriter 类似） */
    void StartPeriodic(EventLoop *loop, double interval_sec = 300.0);

    /**
     * grant/consume 成功后调用。队列超过 10000 条时拒绝并打日志，避免发奖洪水吃光内存。
     * 返回 false 表示没进队列。
     */
    bool Enqueue(const PendingPlayerItem &item);

    /** 登录成功等：加入本地在线集合（Redis 仍为准） */
    void MarkOnline(uint64_t player_id);

    /** 登出：先刷该玩家队列，再从本地在线集合移除 */
    void MarkOffline(uint64_t player_id);

    /**
     * 停机：请求最后一次在线刷盘并等待。
     * 超时则打日志，队列仍留在内存里由调用方决定，不会悄悄丢掉而不留记录。
     */
    void Stop(std::chrono::milliseconds timeout = std::chrono::milliseconds(3000));

    size_t QueueSize() const;

private:
    PlayerItemPersistQueue() = default;
    ~PlayerItemPersistQueue();
    void EnsureWorker();
    void WorkerLoop();
    void OnFlushTick();
    void FlushOnlinePlayers();
    /** 刷某玩家积压到 MySQL */
void FlushPlayer(uint64_t player_id);

    mutable std::mutex mu_;
    std::condition_variable cv_;
    std::thread worker_;
    bool stopped_ = false;
    bool flush_requested_ = false;
    bool flush_finished_ = false;
    std::deque<PendingPlayerItem> queue_;
    /** Redis 不可用时的在线玩家兜底集合 */
    std::unordered_set<uint64_t> online_local_;
};
