#pragma once

/**
 * @file Latch.h
 * @brief 倒计时门闩：CountDown 减到 0 时唤醒 Wait
 *
 * 用于 AsyncLogging 启动同步、测试等待后台就绪。
 */

#include "common.h"
#include <thread>
#include <condition_variable>
class Latch
{
private:
    std::mutex mux_;
    std::condition_variable cv_;
    int count_;

public:
    DISALLOW_COPY_AND_MOVE(Latch);

    explicit Latch(int count) : count_(count){}
    /** 阻塞直到 count_==0 */
    void wait(){
        std::unique_lock<std::mutex> lock(mux_);
        while(count_ > 0){
            cv_.wait(lock);
        }
    }

    /** count_--；到 0 时 notify_all */
    void notify(){
        std::unique_lock<std::mutex> lock(mux_);
        --count_;
        if(count_ == 0){
            cv_.notify_all();
        }
    }
};