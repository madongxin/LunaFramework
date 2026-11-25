#pragma once

/**
 * @file HttpServer.h
 * @brief 基于 TcpServer 的 HTTP 服务（管理口 /metrics、/monitor 等）
 *
 * 数据路径：
 *   TcpConnection::on_message → onMessage 喂 HttpContext 解析
 *   → 完整请求后 onRequest（Formal/token/MONITOR_OPEN 鉴权）
 *   → response_callback_（AdminHttpRouter）填 HttpResponse → Send
 *
 * 默认绑定见 RunServer（Formal 下多为 127.0.0.1；gamedb-0 可 0.0.0.0）。
 */

#include <functional>
#include <memory>
#include <stdio.h>
#include "common.h"

/** 空闲连接自动关闭秒数（ActiveCloseConn 定时检查） */
#define AUTOCLOSETIMEOUT 100

class TimeStamp;
class TcpServer;
class TcpConnection;
class HttpRequest;
class HttpResponse;
class EventLoop;

class HttpServer {
public:
    typedef std::shared_ptr<TcpConnection> TcpConnectionPtr;
    /** 业务路由回调：已鉴权通过后的请求处理 */
    typedef std::function<void(const HttpRequest &, HttpResponse *)> HttpResponseCallback;

    DISALLOW_COPY_AND_MOVE(HttpServer);

    /**
     * @param loop 主 EventLoop（accept 与定时器挂在此；Loop 由调用方驱动）
     * @param ip/port 监听地址
     * @param auto_close_conn 是否按 AUTOCLOSETIMEOUT 主动关空闲连接
     */
    HttpServer(EventLoop *loop, const char *ip, const int port, bool auto_close_conn = false);
    ~HttpServer();

    /** 未设置业务回调时的默认 404 */
    void HttpDefaultCallBack(const HttpRequest &request, HttpResponse *resp);

    /** 设置路由回调（通常为 HttpResponseCallback / AdminHttpRouter） */
    void SetHttpCallback(const HttpResponseCallback &cb);

    /**
     * 拉起 HTTP 子 IO 线程；不阻塞。
     * 进程主循环请对构造时传入的 EventLoop 调用 Loop()。
     */
    void Start();

    /** 新连接建立：可选挂上自动关闭定时器 */
    void onConnection(const TcpConnectionPtr &conn);
    /** 可读：把数据喂给 HttpContext，凑齐请求后调 onRequest */
    void onMessage(const TcpConnectionPtr &conn);
    /**
     * 完整 HTTP 请求入口：EvaluateHttpAdminAccess / MONITOR_OPEN，
     * 通过后调 response_callback_，再 Send 响应。
     */
    void onRequest(const TcpConnectionPtr &conn, const HttpRequest &request);

    /** 子 IO 线程数（转给 TcpServer::SetThreadNums） */
    void SetThreadNums(int thread_nums);

    /** 定时检查空闲连接；不延长 TcpConnection 生命周期（weak_ptr） */
    void ActiveCloseConn(std::weak_ptr<TcpConnection> &conn);

private:
    EventLoop *loop_;
    std::unique_ptr<TcpServer> server_;
    bool auto_close_conn_;
    HttpResponseCallback response_callback_;
};
