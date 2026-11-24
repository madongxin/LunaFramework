#pragma once

#include "HttpRequest.h"
#include "HttpResponse.h"

/**
 * HTTP 管理口统一路由入口（挂到 HttpServer::SetHttpCallback）。
 *
 * 覆盖：/metrics、/live、/ready、/monitor、/api/*、/gm（需 ENABLE_ADMIN）等。
 * 访问控制在 HttpServer 层（Formal + token / MONITOR_OPEN）；本文件只做路径分发。
 */
void HttpResponseCallback(const HttpRequest &request, HttpResponse *response);
