#pragma once


/**
 * CurrentThread.h — 当前线程 tid / 线程名缓存，供 EventLoop 断言「必须在 loop 线程」使用。
 * 路径: base/CurrentThread.h
 */
#include <stdint.h>
#include <pthread.h>
#include <stdio.h>
namespace CurrentThread
{
    extern __thread int t_cachedTid;
    extern __thread char t_formattedTid[32];
    extern __thread int t_formattedTidLength;

    void CacheTid();

    pid_t gettid();

    inline int tid() {
        if (__builtin_expect(t_cachedTid == 0, 0)) {
            CacheTid();
        }
        return t_cachedTid;
    }


    inline const char *tidString() { return t_formattedTid; }
    inline int tidStringLength() { return t_formattedTidLength; }
}