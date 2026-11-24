#pragma once


/**
 * @file NatsThinClient.h
 * @brief 轻量 NATS 客户端（可选消息总线）
 *
 * 未配置时调用失败/空操作；不阻塞主登录路径。
 */
#include <mutex>
#include <string>

/** 最小 NATS PUB 客户端（无 libnats）；url 空则禁用 */
class NatsThinClient {
public:
    static NatsThinClient &Instance();
    void SetUrl(std::string url);  // nats://host:port
    bool enabled() const;
    /** 发布成功返回 true；未启用返回 false（调用方走本地路径） */
    bool Publish(const std::string &subject, const std::string &payload);

private:
    NatsThinClient() = default;
    bool EnsureConnected();
    void Disconnect();

    mutable std::mutex mu_;
    std::string url_;
    std::string host_ = "127.0.0.1";
    int port_ = 4222;
    int fd_ = -1;
};
