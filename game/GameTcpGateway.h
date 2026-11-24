#pragma once

/**
 * @file GameTcpGateway.h
 * @brief 游戏 TCP 网关：独立线程 + EventLoop，接收客户端 length-prefix protobuf
 *
 * 接收链路（自底向上）：
 *   Acceptor accept → TcpServer 绑子 EventLoop → epoll 可读
 *   → TcpConnection::HandleMessage → OnMessage 追加流式缓冲
 *   → ProtoFraming 拆帧 → 按 player 投递串行队列
 *   → GameService::HandleFrame / 登录编排 → ReplySink 回投 IO 线程 Send
 *
 * 协议：4 字节大端长度 + GameRequest/GameResponse（见 ProtoFraming）。
 * 独立 std::thread（StartInBackground），与 HTTP 主 EventLoop 分离；
 * IO 线程不跑 GameLogic；Login/Reconnect 绑 conn↔player；
 * 断线 MarkDisconnected（宽限重连）；StopAccepting/RequestQuit 配合 SIGTERM 摘流。
 */

#include <atomic>
#include <memory>
#include <string>
#include <thread>

class EventLoop;
class TcpConnection;
class TcpServer;

class GameTcpGateway {
public:
    /**
     * @param ip/port 公网或 VIP 后监听地址（正式拓扑多为 VIP:8081 后端）
     * @param instance_id 稳定 Gateway 身份（如 gw-0），不得用 listen 地址拼接
     */
    GameTcpGateway(const std::string &ip, int port, const std::string &instance_id);
    ~GameTcpGateway();

    /** 在后台线程启动 Run()，内部阻塞在 EventLoop::Loop 直至 RequestQuit。 */
    void StartInBackground();

    /** 停止 accept（跨线程安全：投递到 gateway loop，摘流前调用）。 */
    void StopAccepting();
    /** 退出 gateway EventLoop；随后可 join 后台线程。 */
    void RequestQuit();

    /** 本实例稳定身份，供 Session Bind / Push 定向。 */
    const std::string &gateway_instance_id() const { return instance_id_; }
    /** 进程内是否已有 Gateway 在监听（健康检查 / 单测）。 */
    static bool TcpListening();

private:
    /** 线程入口：建 EventLoop + TcpServer，注册 OnMessage/关闭回调后 Loop。 */
    void Run();
    /**
     * TcpServer message_callback：粘包拆帧、预鉴权/登录编排或业务转发、回包。
     * 不得在此同步跑重 GameLogic；已登录帧投递串行队列。
     */
    void OnMessage(const std::shared_ptr<TcpConnection> &conn);

    std::string ip_;
    int port_ = 0;
    std::string instance_id_;
    std::thread thread_;
    std::atomic<EventLoop *> loop_{nullptr};
    std::atomic<TcpServer *> server_{nullptr};
};
