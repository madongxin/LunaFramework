#pragma once

/**
 * @file TimerQueue.h
 * @brief 属于某个 EventLoop 的定时器队列（timerfd + set 按到期排序）
 *
 * EventLoop::RunAt/After/Every 最终落到 AddTimer。
 * timers_ 只能在 loop 线程修改（跨线程由 EventLoop 先 Queue 再调）。
 */

#include <unistd.h>
#include <sys/timerfd.h>
#include <set>
#include <vector>
#include <memory>
#include <utility>
#include <functional>
#include "common.h"
#include "TimeStamp.h"

class Timer;
class EventLoop;
class Channel;

class TimerQueue {
public:
    DISALLOW_COPY_AND_MOVE(TimerQueue)
    explicit TimerQueue(EventLoop *loop);
    ~TimerQueue();

    /** 创建非阻塞 timerfd 并挂到 loop 的 Channel 上 */
    void CreateTimerfd();

    /** 消费 timerfd 可读事件（drain 计数） */
    void ReadTimerFd();
    /** timerfd 可读：取出所有到期 Timer，run，再 ResetTimers */
    void HandleRead();

    /** 把 timerfd 下次超时设为该 Timer 的 expiration */
    void ResetTimerFd(Timer *timer);
    /** 周期任务 ReStart 后插回；一次性任务 delete */
    void ResetTimers();

    /**
     * 插入 timers_；若成为新的最早到期则返回 true（调用方应 ResetTimerFd）。
     */
    bool Insert(Timer *timer);
    /**
     * 新建 Timer 并 Insert；必须在所属 EventLoop 线程调用。
     * @param interval 秒；0 为单次
     */
    void AddTimer(TimeStamp timestamp, std::function<void()> const &cb, double interval);

private:
    typedef std::pair<TimeStamp, Timer *> Entry;

    EventLoop *loop_;
    int timerfd_;
    std::unique_ptr<Channel> channel_;
    std::set<Entry> timers_;
    std::vector<Entry> active_timers_;
};
