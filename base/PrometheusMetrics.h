#pragma once


/**
 * PrometheusMetrics.h — 汇总各模块指标为 Prometheus exposition 文本。
 * 路径: base/PrometheusMetrics.h
 */
#include <string>

std::string BuildPrometheusMetricsText();
