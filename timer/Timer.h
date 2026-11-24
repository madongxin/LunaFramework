#pragma once

/**
 * @file Timer.h
 * @brief 单个定时任务：到期绝对时间 + 回调 + 可选周期间隔
 *
 * 由 TimerQueue 持有；到期后 run()；若 repeat_ 则 ReStart 再插回队列。
 */

#include <functional>
#include "common.h"
#include "TimeStamp.h"

class Timer {
public:
    DISALLOW_COPY_AND_MOVE(Timer);

    /**
     * @param timestamp 首次到期绝对时间
     * @param cb 到期回调（在所属 EventLoop 线程执行）
     * @param interval 秒；>0 表示周期定时器
     */
    Timer(TimeStamp timestamp, std::function<void()> const &cb, double interval);

    /** 周期定时器：以 now 为基准加上 interval_ 得到下次到期 */
    void ReStart(TimeStamp now);

    /** 执行用户回调 */
    void run() const;
    TimeStamp expiration() const;
    bool repeat() const;

private:
    TimeStamp expiration_;
    std::function<void()> callback_;
    double interval_;
    bool repeat_;
};
