#pragma once

#include <string>

#include <json/json.h>

#include "HttpRequest.h"
#include "HttpResponse.h"

std::string NewGmSessionId();
bool GmSessionValid(const std::string &sid);
std::string GmSidFromRequest(const HttpRequest &request);
std::string GmConsolePage(const HttpRequest &request, const std::string &sid);
Json::Value GmExecute(const std::string &cmd);

Json::Value BuildFilesJson();
std::string BuildFileHtml();
bool DownloadFile(const std::string &name, HttpResponse *response, bool head_only);

void crash_uaf();
