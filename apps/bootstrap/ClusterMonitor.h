#pragma once

#include <json/json.h>

#include "HttpRequest.h"
#include "HttpResponse.h"

Json::Value BuildClusterStatusJson();
Json::Value BuildMonitorLiveJson();
void HandlePrometheusInstant(const HttpRequest &request, HttpResponse *response, bool head_only);
void HandlePrometheusRange(const HttpRequest &request, HttpResponse *response, bool head_only);
