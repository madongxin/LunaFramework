#pragma once


/**
 * @file EtcdDiscovery.h
 * @brief 可选 etcd v2 HTTP 服务发现
 *
 * endpoints 空则 no-op；正式拓扑可关 etcd 走静态/Redis Registry。
 */
#include <string>
#include <vector>

/**
 * 可选 etcd v2 HTTP 发现（endpoints 空则全部 no-op / 失败）。
 * 注册：PUT /v2/keys/{prefix}/{service}/{id}?ttl=
 * 发现：GET /v2/keys/{prefix}/{service} → 解析 value 为 addr 列表
 */
class EtcdDiscovery {
public:
    static EtcdDiscovery &Instance();

    void Configure(std::string endpoints, std::string prefix = "lunaserver");
    bool enabled() const;

    bool Register(const std::string &service, const std::string &instance_id,
                  const std::string &addr, int ttl_sec = 30);
    bool Discover(const std::string &service, std::vector<std::string> *addrs);

private:
    EtcdDiscovery() = default;
    bool HttpPut(const std::string &path, const std::string &body, std::string *resp);
    bool HttpGet(const std::string &path, std::string *resp);

    std::string host_ = "127.0.0.1";
    int port_ = 2379;
    std::string prefix_ = "lunaserver";
    bool enabled_ = false;
};
