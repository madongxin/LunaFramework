#pragma once

/**
 * @file BrpcGameServer.h
 * @brief 进程内 brpc Server：注册 MailBrpcService，与 GameTcpGateway 双栈并存
 *
 * 旧单进程 role=all 路径使用；读 config/brpc.cnf 启动。
 * 默认绑定 127.0.0.1；Formal 拒绝 0.0.0.0/公网，除非 allow_public_bind。
 */

#include <memory>
#include <string>

namespace brpc {
class Server;
}

class MailBrpcServiceImpl;

class BrpcGameServer {
public:
    static BrpcGameServer &Instance();

    /** 读 config/brpc.cnf 并 Start；失败返回 false。 */
    bool StartFromConfig();

    /**
     * 指定地址启动并注册 MailBrpcServiceImpl。
     * 未配置时用 127.0.0.1；正式模式拒绝 0.0.0.0 和公网地址。
     */
    bool Start(const std::string &listen_addr, int idle_timeout_sec = 30,
               bool allow_public_bind = false);

    /** 停止 brpc Server 并释放 service；可重复调用。 */
    void Stop();
    bool running() const { return running_; }
    const std::string &listen_addr() const { return listen_addr_; }

private:
    BrpcGameServer() = default;
    ~BrpcGameServer();

    std::unique_ptr<brpc::Server> server_;
    std::unique_ptr<MailBrpcServiceImpl> service_;
    std::string listen_addr_;
    bool running_ = false;
};
