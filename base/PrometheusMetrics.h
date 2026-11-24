#pragma once

/**
 * @file PrometheusMetrics.h
 * @brief 汇总各模块指标为 Prometheus exposition 文本
 *
 * /metrics 或监控页拉取；内容含 process、eventloop、packet、logic 等。
 */

#include <string>

/** 拼装完整 Prometheus text；供 /metrics 或监控转发 */
std::string BuildPrometheusMetricsText();
