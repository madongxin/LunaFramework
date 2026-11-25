/**
 * 管理 HTTP 响应拼装与 URL 参数工具实现。
 * 不负责鉴权（见 HttpServer）与路由（见 AdminHttpRouter）。
 */
#include "bootstrap/AdminHttpUtil.h"

#include <cctype>
#include <fstream>
#include <sstream>

std::string ReadFile(const std::string &path) {
    std::ifstream in(path, std::ios::binary);
    if (!in)
        return "";
    return std::string((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
}

void SendHtml(HttpResponse *response, const std::string &body, bool head_only) {
    response->SetStatusCode(HttpResponse::HttpStatusCode::k200K);
    response->SetStatusMessage("OK");
    response->SetContentType("text/html; charset=utf-8");
    // HEAD：只回头部，body 长度为 0
    response->SetContentLength(static_cast<int>(head_only ? 0 : body.size()));
    response->SetBody(head_only ? "" : body);
}

void SendPlain(HttpResponse *response, const std::string &body,
               HttpResponse::HttpStatusCode code) {
    response->SetStatusCode(code);
    if (code == HttpResponse::HttpStatusCode::k200K)
        response->SetStatusMessage("OK");
    else if (code == HttpResponse::HttpStatusCode::k400BadRequest)
        response->SetStatusMessage("Bad Request");
    else if (code == HttpResponse::HttpStatusCode::k403Forbidden)
        response->SetStatusMessage("Forbidden");
    else if (code == HttpResponse::HttpStatusCode::k404NotFound)
        response->SetStatusMessage("Not Found");
    else
        response->SetStatusMessage("Error");
    response->SetContentType("text/plain; charset=utf-8");
    response->SetContentLength(static_cast<int>(body.size()));
    response->SetBody(body);
}

void SendJson(HttpResponse *response, const Json::Value &v,
              HttpResponse::HttpStatusCode code) {
    Json::StreamWriterBuilder b;
    b["indentation"] = "";
    const std::string body = Json::writeString(b, v);
    response->SetStatusCode(code);
    // 非 2xx 不得写死 OK，避免客户端/代理误判成功
    if (code == HttpResponse::HttpStatusCode::k200K)
        response->SetStatusMessage("OK");
    else if (code == HttpResponse::HttpStatusCode::k400BadRequest)
        response->SetStatusMessage("Bad Request");
    else if (code == HttpResponse::HttpStatusCode::k403Forbidden)
        response->SetStatusMessage("Forbidden");
    else if (code == HttpResponse::HttpStatusCode::k404NotFound)
        response->SetStatusMessage("Not Found");
    else
        response->SetStatusMessage("Error");
    response->SetContentType("application/json; charset=utf-8");
    response->SetContentLength(static_cast<int>(body.size()));
    response->SetBody(body);
}

void SendPrometheusMetrics(HttpResponse *response, const std::string &body, bool head_only) {
    response->SetStatusCode(HttpResponse::HttpStatusCode::k200K);
    response->SetStatusMessage("OK");
    // Prometheus 文本 exposition 约定的 Content-Type
    response->SetContentType("text/plain; version=0.0.4; charset=utf-8");
    response->SetContentLength(static_cast<int>(head_only ? 0 : body.size()));
    response->SetBody(head_only ? "" : body);
}

/** 从 HttpRequest 已解析的 request_params 取查询参数（非原始 query string）。 */
std::string UrlParam(const HttpRequest &request, const char *key) {
    const auto &m = request.request_params();
    const auto it = m.find(key);
    return it != m.end() ? it->second : "";
}

/** application/x-www-form-urlencoded 风格解码（+ → 空格，%XX）。 */
std::string UrlDecodeParam(std::string s) {
    std::string out;
    out.reserve(s.size());
    auto hex = [](char c) -> int {
        if (c >= '0' && c <= '9')
            return c - '0';
        if (c >= 'a' && c <= 'f')
            return c - 'a' + 10;
        if (c >= 'A' && c <= 'F')
            return c - 'A' + 10;
        return -1;
    };
    for (size_t i = 0; i < s.size(); ++i) {
        if (s[i] == '+')
            out.push_back(' ');
        else if (s[i] == '%' && i + 2 < s.size()) {
            const int hi = hex(s[i + 1]);
            const int lo = hex(s[i + 2]);
            if (hi >= 0 && lo >= 0) {
                out.push_back(static_cast<char>((hi << 4) | lo));
                i += 2;
            } else
                out.push_back(s[i]);
        } else
            out.push_back(s[i]);
    }
    return out;
}
