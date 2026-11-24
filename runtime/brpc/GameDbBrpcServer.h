#pragma once

/**
 * @file GameDbBrpcServer.h
 * @brief GameDB brpc Server 启动封装
 *
 * Formal 下可按需 allow_public_bind；注册 GameDbServiceImpl。
 */
#include <memory>
#include <string>

namespace brpc {
class Server;
}

class GameDbServiceImpl;

class GameDbBrpcServer {
public:
    static GameDbBrpcServer &Instance();
    bool StartFromConfig();
    bool Start(const std::string &listen_addr, int idle_timeout_sec = 30);
    void Stop();
    bool running() const { return running_; }
    const std::string &listen_addr() const { return listen_addr_; }

private:
    GameDbBrpcServer() = default;
    ~GameDbBrpcServer();
    std::unique_ptr<brpc::Server> server_;
    std::unique_ptr<GameDbServiceImpl> service_;
    std::string listen_addr_;
    bool running_ = false;
};
