#pragma once

#include <json/json.h>

#include "HttpRequest.h"
#include "HttpResponse.h"

/**
 * 集群观察页后端：读 inventory.tsv、探活 /live|/ready、代理 Prometheus 查询。
 * 典型路径：/api/cluster、/api/monitor/live、/api/prometheus/query(_range)。
 * Prom 查询有并发槽位限制，避免占满 gamedb HTTP 线程导致 /monitor 本身排队。
 */

/** 进程清单 + alive/ready 汇总（监控页左侧集群表）。 */
Json::Value BuildClusterStatusJson();
/** 网关在线、出口、Move p99 等实时摘要（监控页顶部状态卡）。 */
Json::Value BuildMonitorLiveJson();
void HandlePrometheusInstant(const HttpRequest &request, HttpResponse *response, bool head_only);
void HandlePrometheusRange(const HttpRequest &request, HttpResponse *response, bool head_only);
