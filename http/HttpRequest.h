#pragma once

/**
 * @file HttpRequest.h
 * @brief 已解析的 HTTP 请求对象（由 HttpContext 状态机填充）
 *
 * 注意：url() 通常不含查询串；query 参数在 request_params_ 中。
 */

#include <string>
#include <map>

class HttpRequest {
public:
    enum Method { kInvalid = 0, kGet, kPost, kHead, kPut, kDelete };
    enum Version { kUnknown = 0, kHttp10, kHttp11 };

    HttpRequest();
    ~HttpRequest();

    /** 设置/读取 HTTP 版本（如 "HTTP/1.1"） */
    void SetVersion(const std::string &ver);
    Version version() const;
    std::string GetVersionString() const;

    /** 解析方法字符串；非法则返回 false 并保持 kInvalid */
    bool SetMethod(const std::string &method);
    Method method() const;
    std::string GetMethodString() const;

    /** 请求路径（不含 ?query） */
    void SetUrl(const std::string &url);
    const std::string &url() const;

    /** URL / 表单解析出的参数 */
    void SetRequestParams(const std::string &key, const std::string &value);
    std::string GetRequestValue(const std::string &key) const;
    const std::map<std::string, std::string> &request_params() const;

    void SetProtocol(const std::string &str);
    const std::string &protocol() const;

    /** 请求头（字段名按实现约定大小写） */
    void AddHeader(const std::string &field, const std::string &value);
    std::string GetHeader(const std::string &field) const;
    const std::map<std::string, std::string> &headers() const;

    void SetBody(const std::string &str);
    const std::string &body() const;

private:
    Method method_;
    Version version_;
    std::map<std::string, std::string> request_params_;
    std::string url_;
    std::string protocol_;
    std::map<std::string, std::string> headers_;
    std::string body_;
};
