#pragma once

/**
 * @file ResourceWatchdog.h
 * @brief 周期采样 RSS/fd/TCP 队列，供 GM 启发式告警
 *
 * 挂在 EventLoop 定时器上；历史样本供「缓增、暴涨、fd 爬升」判断。
 */

#include <json/json.h>
#include <mutex>
#include <vector>

class EventLoop;

class ResourceWatchdog {
public:
    struct Sample {
        int64_t time_unix = 0;
        int64_t rss_kb = 0;
        int64_t vm_size_kb = 0;
        int fds = 0;
        uint64_t tcp_send_q = 0;
        uint64_t tcp_recv_q = 0;
    };

    /** 进程单例 */
    static ResourceWatchdog &Instance();

    /** 在 loop 上挂周期定时器，每 interval_sec 秒 PushSample */
    void StartPeriodic(EventLoop *loop, double interval_sec);

    /** 立刻采一次 /proc 并写入 samples_ 环形历史 */
    void PushSample();

    /** 最近一条样本的 JSON */
    Json::Value LastSampleJson() const;
    /** 启发式：缓增/暴涨/fd 爬升/队列爬升等告警标志 */
    Json::Value AnalyzeTrends() const;
    /** 返回至多 max_points 条历史样本 */
    Json::Value HistoryJson(int max_points) const;

private:
    ResourceWatchdog() = default;

    mutable std::mutex mutex_;
    std::vector<Sample> samples_;
    static constexpr size_t kMaxSamples = 512;
};
