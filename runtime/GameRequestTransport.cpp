/**
 * @file GameRequestTransport.cpp
 * @brief GameRequestTransport 实现：线程安全的全局 ITransport 指针
 *
 * 未 Set 时 Get 回落 InProcessTransport::Instance()，保证单进程路径可直接跑通。
 */

#include "GameRequestTransport.h"

#include "InProcessTransport.h"

std::mutex GameRequestTransport::mu_;
ITransport *GameRequestTransport::transport_ = nullptr;

/** 注册或清除传输后端；启动路径调用。 */
void GameRequestTransport::Set(ITransport *transport) {
    std::lock_guard<std::mutex> lk(mu_);
    transport_ = transport;
}

/** 取当前后端；空指针时回落进程内实现。 */
ITransport &GameRequestTransport::Get() {
    std::lock_guard<std::mutex> lk(mu_);
    if (transport_)
        return *transport_;
    return InProcessTransport::Instance();
}
