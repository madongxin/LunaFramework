#pragma once

/**
 * @file GameRequestTransport.h
 * @brief 进程内当前游戏请求传输后端的全局注册点
 *
 * 启动时 Set(InProcessTransport 或 BrpcTransport)；业务路径 Get() 取当前实现。
 * 未 Set 时 Get 回落到 InProcessTransport::Instance()，便于单进程联调。
 *
 * 协作：Gateway/编排层只依赖 ITransport；具体后端由进程角色注入。
 */

#include "ITransport.h"

#include <memory>
#include <mutex>

/** 进程内当前游戏请求传输后端（InProcess 或 Brpc）。 */
class GameRequestTransport {
public:
    /**
     * 注册传输后端（可为 nullptr 表示清除，之后 Get 回落 InProcess）。
     * 线程安全；通常仅在启动路径调用一次。
     */
    static void Set(ITransport *transport);
    /**
     * 取得当前后端；未 Set 时返回 InProcessTransport 单例。
     * 返回引用的生命周期由 Set 方或 InProcess 单例保证。
     */
    static ITransport &Get();

private:
    static std::mutex mu_;
    static ITransport *transport_;
};
