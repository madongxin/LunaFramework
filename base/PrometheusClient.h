#pragma once


/**
 * PrometheusClient.h — 本机 Prometheus HTTP 客户端（监控页 query 转发）。
 * 路径: base/PrometheusClient.h
 */
#include <string>

struct PrometheusClientResult {
    int http_status = 0;
    std::string body;
    std::string error;
};

PrometheusClientResult PrometheusInstantQuery(const std::string &query);
PrometheusClientResult PrometheusQueryRange(const std::string &query, const std::string &start,
                                            const std::string &end, const std::string &step);
