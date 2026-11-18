#pragma once

#include <string>

#include <json/json.h>

#include "HttpRequest.h"
#include "HttpResponse.h"

std::string ReadFile(const std::string &path);

void SendHtml(HttpResponse *response, const std::string &body, bool head_only);
void SendPlain(HttpResponse *response, const std::string &body,
               HttpResponse::HttpStatusCode code = HttpResponse::HttpStatusCode::k200K);
void SendJson(HttpResponse *response, const Json::Value &v,
              HttpResponse::HttpStatusCode code = HttpResponse::HttpStatusCode::k200K);
void SendPrometheusMetrics(HttpResponse *response, const std::string &body, bool head_only);

std::string UrlParam(const HttpRequest &request, const char *key);
std::string UrlDecodeParam(std::string s);
