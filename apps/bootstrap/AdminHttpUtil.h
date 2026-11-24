#pragma once

#include <string>

#include <json/json.h>

#include "HttpRequest.h"
#include "HttpResponse.h"

/**
 * 管理 HTTP 的小工具：读静态文件、组 HTML/JSON/Prometheus 响应、解析 URL 参数。
 * 供 AdminHttpRouter / GmAdmin / ClusterMonitor 共用。
 */

std::string ReadFile(const std::string &path);

void SendHtml(HttpResponse *response, const std::string &body, bool head_only);
void SendPlain(HttpResponse *response, const std::string &body,
               HttpResponse::HttpStatusCode code = HttpResponse::HttpStatusCode::k200K);
void SendJson(HttpResponse *response, const Json::Value &v,
              HttpResponse::HttpStatusCode code = HttpResponse::HttpStatusCode::k200K);
/** Content-Type: text/plain; version=0.0.4（Prometheus 文本格式） */
void SendPrometheusMetrics(HttpResponse *response, const std::string &body, bool head_only);

std::string UrlParam(const HttpRequest &request, const char *key);
std::string UrlDecodeParam(std::string s);
