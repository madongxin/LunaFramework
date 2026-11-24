#pragma once

/**
 * @file PasswordExec.h
 * @brief 口令 PBKDF2 卸载队列：把重 CPU 哈希挪出 brpc 工作线程
 *
 * Post 投递任务到固定 worker 池；Stop/析构先停循环再 join，禁止 detach。
 * Auth Login/Register 在验证密码前应 Post，避免堵住 RPC 线程影响其它请求。
 *
 * 协作：PasswordHash；AuthServiceImpl。
 */

#include <condition_variable>
#include <deque>
#include <functional>
#include <mutex>
#include <thread>
#include <vector>

/** 把 PBKDF2 挪出 brpc 工作线程。Stop() 先停循环再 join，析构同样如此。 */
class PasswordExec {
public:
    static PasswordExec &Instance();

    /**
     * 投递异步任务；已 Stop 或队列异常时返回 false。
     * fn 内可调用 PasswordHash，完成后自行 Done RPC。
     */
    bool Post(std::function<void()> fn);
    /** 停止接受新任务并 join 全部 worker。 */
    void Stop();

private:
    PasswordExec();
    ~PasswordExec();
    void Loop();

    std::mutex mu_;
    std::condition_variable cv_;
    std::deque<std::function<void()>> q_;
    std::vector<std::thread> workers_;
    bool stopped_ = false;
};
