#pragma once

/**
 * GameRequestTransport.h — 请求在 Gateway/Logic 之间的传输上下文。
 * 路径: runtime/GameRequestTransport.h
 */
#include "ITransport.h"

#include <memory>
#include <mutex>

/** 进程内当前游戏请求传输后端（InProcess 或 Brpc） */
class GameRequestTransport {
public:
    static void Set(ITransport *transport);
    static ITransport &Get();

private:
    static std::mutex mu_;
    static ITransport *transport_;
};
