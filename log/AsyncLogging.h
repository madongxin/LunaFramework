#pragma once

/**
 * @file AsyncLogging.h
 * @brief 双缓冲异步日志：前台 Append，后台线程批量写 LogFile
 *
 * current_/next_ 交换；满缓冲进 buffers_，ThreadFunc 唤醒后落盘。
 * 与 Logger::setOutput 对接后，业务线程不阻塞在 fwrite。
 */

#include <vector>
#include <memory>
#include <thread>
#include "Latch.h"
#include "Logging.h"

static const double BufferWriteTimeout = 3.0;
static const int64_t FileMaximumSize = 1024 * 1024 * 1024;

class AsyncLogging {
public:
    typedef FixedBuffer<FixedLargeBuffferSize> Buffer;

    /** @param filepath 日志文件路径前缀；空则用默认命名 */
    explicit AsyncLogging(const char *filepath = nullptr);
    ~AsyncLogging();

    void Stop();
    /** 启动后台线程；Latch 等待线程就绪 */
    void Start();

    /** 前台：把一行日志追加进 current_（可能交换缓冲并 notify） */
    void Append(const char *data, int len);
    /** 请求尽快刷盘（实现可能仅 notify） */
    void Flush();
    /** 后台循环：取满缓冲 → 写文件 → 滚动 */
    void ThreadFunc();

private:
    bool running_;
    const char *filepath_;
    std::mutex mutex_;
    std::condition_variable cv_;
    Latch latch_;
    std::thread thread_;

    std::unique_ptr<Buffer> current_;
    std::unique_ptr<Buffer> next_;
    std::vector<std::unique_ptr<Buffer>> buffers_;
};
