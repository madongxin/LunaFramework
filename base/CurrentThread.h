#pragma once

/**
 * @file CurrentThread.h
 * @brief 当前线程 tid 缓存（__thread），供 EventLoop::IsInLoopThread
 *
 * 首次调用 CacheTid 读 syscall；之后用 t_cachedTid，避免反复 gettid。
 */

#include <stdint.h>
#include <pthread.h>
#include <stdio.h>
namespace CurrentThread
{
    extern __thread int t_cachedTid;
    extern __thread char t_formattedTid[32];
    extern __thread int t_formattedTidLength;

    /** 若未缓存则 syscall gettid 写入 t_cachedTid */
    void CacheTid();

    /** 原始 gettid 系统调用 */
    pid_t gettid();

    /** 返回缓存 tid；首次触发 CacheTid */
    inline int tid() {
        if (__builtin_expect(t_cachedTid == 0, 0)) {
            CacheTid();
        }
        return t_cachedTid;
    }


    inline const char *tidString() { return t_formattedTid; }
    inline int tidStringLength() { return t_formattedTidLength; }
}