/**
 * @file ForwardMetaContext.cpp
 * @brief ForwardMetaContext 实现：转发元数据线程局部/上下文传递
 */

#include "ForwardMetaContext.h"

namespace {
thread_local bool g_has = false;
thread_local ForwardRouteMeta g_meta;
}

void ForwardMetaContext::Set(const ForwardRouteMeta &meta) {
    g_meta = meta;
    g_has = true;
}

void ForwardMetaContext::Clear() {
    g_has = false;
    g_meta = ForwardRouteMeta{};
}

const ForwardRouteMeta *ForwardMetaContext::Get() {
    return g_has ? &g_meta : nullptr;
}
