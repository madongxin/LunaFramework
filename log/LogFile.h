#pragma once

/**
 * @file LogFile.h
 * @brief 日志文件写入与按间隔 flush（供 AsyncLogging 后台使用）
 */

#include <stdio.h>
#include <unistd.h>
#include <sys/time.h>
#include <string>
#include "TimeStamp.h"

static const time_t FlushInterval = 3;

class LogFile {
public:
    explicit LogFile(const char *filepath = nullptr);
    ~LogFile();

    /** 追加写入；内部可按大小/时间触发 flush */
    void Write(const char *data, int len);
    void Flush();
    int64_t writtenbytes() const;

private:
    FILE *fp_;
    int64_t written_bytes_;
    time_t lastwrite_;
    time_t lastflush_;
};
