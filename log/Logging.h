#pragma once

/**
 * @file Logging.h
 * @brief 日志门面：级别过滤、流式拼装、可替换 output/flush
 *
 * 用法：LOG_INFO << "x=" << x;
 * 析构 Impl 时把缓冲交给全局 OutputFunc（正式环境多为 AsyncLogging::Append）。
 * SetLogService/Player/Session/ErrorCode 供 JSON 结构化字段。
 */

#include <string.h>
#include "common.h"
#include "TimeStamp.h"
#include "LogStream.h"

class Logger {
public:
    DISALLOW_COPY_AND_MOVE(Logger);

    enum LogLevel { DEBUG, INFO, WARN, ERROR, FATAL };

    /** 从 __FILE__ 截取 basename，避免整路径过长 */
    class SourceFile {
    public:
        SourceFile(const char *data);
        const char *data_;
        int size_;
    };

    Logger(const char *file_, int line, LogLevel level);
    ~Logger();

    /** 返回拼装流，供 LOG_* 宏链式写入 */
    LogStream &stream();

    static LogLevel logLevel();
    static void setLogLevel(LogLevel level);

    typedef void (*OutputFunc)(const char *data, int len);
    typedef void (*FlushFunc)();
    /** 默认 fwrite stdout；RunServer 可换成 AsyncLogging */
    static void setOutput(OutputFunc);
    static void setFlush(FlushFunc);

private:
    /** 组装时间、级别、正文、源文件行号 */
    class Impl {
    public:
        DISALLOW_COPY_AND_MOVE(Impl);
        typedef Logger::LogLevel LogLevel;
        Impl(Logger::LogLevel level, const SourceFile &source, int line);
        void FormattedTime();
        void Finish();
        LogStream &stream();
        const char *loglevel() const;
        LogLevel level_;

    private:
        Logger::SourceFile sourcefile_;
        int line_;
        LogStream stream_;
    };
    Impl impl_;
};

extern Logger::LogLevel g_logLevel;
inline Logger::LogLevel Logger::logLevel() { return g_logLevel; }

#define LOG_DEBUG                          \
    if (Logger::logLevel() <= Logger::DEBUG) \
    Logger(__FILE__, __LINE__, Logger::DEBUG).stream()
#define LOG_INFO                         \
    if (Logger::logLevel() <= Logger::INFO) \
    Logger(__FILE__, __LINE__, Logger::INFO).stream()
#define LOG_WARN Logger(__FILE__, __LINE__, Logger::WARN).stream()
#define LOG_ERROR Logger(__FILE__, __LINE__, Logger::ERROR).stream()
#define LOG_FATAL Logger(__FILE__, __LINE__, Logger::FATAL).stream()

/** 进程级 service 名；JSON 日志带上这些上下文 */
void SetLogService(const char *service);
void SetLogPlayer(uint64_t player_id);
void SetLogSession(const char *session_id);
void SetLogErrorCode(const char *error_code);
