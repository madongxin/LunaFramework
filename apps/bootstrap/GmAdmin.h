#pragma once

#include <string>

#include <json/json.h>

#include "HttpRequest.h"
#include "HttpResponse.h"

/**
 * 危险 HTTP GM 控制台与简易文件服务。
 * 仅当 LUNASERVER_ENABLE_ADMIN=1 时由 AdminHttpRouter 暴露；Formal 默认关闭。
 */

std::string NewGmSessionId();
bool GmSessionValid(const std::string &sid);
std::string GmSidFromRequest(const HttpRequest &request);
std::string GmConsolePage(const HttpRequest &request, const std::string &sid);
/** 执行一条 GM 文本命令，返回 JSON 结果（勿在公网无鉴权环境开启）。 */
Json::Value GmExecute(const std::string &cmd);

Json::Value BuildFilesJson();
std::string BuildFileHtml();
bool DownloadFile(const std::string &name, HttpResponse *response, bool head_only);

/** 仅调试用：故意触发 UAF，勿在生产调用。 */
void crash_uaf();
