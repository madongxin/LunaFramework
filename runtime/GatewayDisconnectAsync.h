#pragma once

/**
 * @file GatewayDisconnectAsync.h
 * @brief 断线后异步 MarkDisconnected，避免堵 IO 线程
 *
 * Gateway 断线本地 Redis 补偿：禁止在 Reactor 线程同步 MarkDisconnected。
 * 有界队列 + 后台 worker；队列满则丢弃并记 dropped 指标。
 * Stop 必须 join，禁止 detach；默认执行器在 stop 后不得再调 SessionStore。
 * MarkDisconnected ≠ Logout——仅标记会话断开，不清理账号登录态。
 *
 * 协作：Gateway 关连路径 EnqueueMarkDisconnected；SessionStore 为默认执行体。
 */

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <thread>

/**
 * Gateway 断线异步补偿队列。
 * Reactor 只入队；worker 串行执行 MarkDisconnected（或测试注入的 executor）。
 */
class GatewayDisconnectAsync {
public:
    /** 单次 MarkDisconnected 任务载荷（player + session fence）。 */
    struct Task {
        uint64_t player_id = 0;
        std::string token;
        uint64_t generation = 0;
    };

    static GatewayDisconnectAsync &Instance();

    /** 启动后台 worker；可指定队列上限（默认 4096）。重复 Start 幂等。 */
    void Start(size_t max_queue = 4096);
    /**
     * 禁止入队、取消剩余队列、join worker。
     * 正在执行的任务必须轮询 stopping() 以便尽快退出；禁止 detach。
     */
    void Stop(std::chrono::milliseconds drain_deadline = std::chrono::milliseconds(2000));

    /**
     * Reactor 线程安全入口：仅入队。
     * 成功 true；队列满、未启动或已 stop 时 false（并累计 dropped）。
     */
    bool EnqueueMarkDisconnected(uint64_t player_id, const std::string &token, uint64_t generation);

    /** 当前排队任务数（近似）。 */
    size_t pending() const;
    uint64_t dropped() const { return dropped_.load(std::memory_order_relaxed); }
    uint64_t cancelled() const { return cancelled_.load(std::memory_order_relaxed); }
    uint64_t executed() const;
    /** worker/执行体应轮询：Stop 已发出时返回 true。 */
    bool stopping() const;

    /**
     * 单测：替换实际执行体（可注入 sleep）；传 nullptr 恢复默认 SessionStore 路径。
     * 生产路径勿调用。
     */
    void SetExecutorForTest(std::function<void(const Task &)> exec);

private:
    struct State {
        std::mutex mu;
        std::condition_variable cv;
        std::deque<Task> q;
        size_t max_queue = 4096;
        std::atomic<bool> stop{true};
        std::atomic<size_t> pending{0};
        std::atomic<uint64_t> executed{0};
        std::atomic<uint64_t> cancelled{0};
        std::function<void(const Task &)> test_exec;
    };

    GatewayDisconnectAsync() = default;
    ~GatewayDisconnectAsync();

    static void WorkerLoop(std::shared_ptr<State> st);
    static void RunDefault(const std::shared_ptr<State> &st, const Task &t);

    mutable std::mutex life_mu_;
    std::shared_ptr<State> state_;
    std::thread worker_;
    std::atomic<uint64_t> dropped_{0};
    std::atomic<uint64_t> executed_{0};
    std::atomic<uint64_t> cancelled_{0};
};
