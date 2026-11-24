#pragma once

/**
 * @file PrometheusClient.h
 * @brief 本机 Prometheus HTTP 客户端（监控页 query / query_range 转发）
 */

#include <string>

struct PrometheusClientResult {
    int http_status = 0;
    std::string body;
    std::string error;
};

/** 即时查询；失败时 error 非空或 http_status!=200 */
PrometheusClientResult PrometheusInstantQuery(const std::string &query);
/** 范围查询；start/end/step 原样传给 Prometheus API */
PrometheusClientResult PrometheusQueryRange(const std::string &query, const std::string &start,
                                            const std::string &end, const std::string &step);
