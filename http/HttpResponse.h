#pragma once

/**
 * @file HttpResponse.h
 * @brief HTTP 响应拼装：状态行、头、body 或文件 fd
 *
 * bodytype=FILE_TYPE 时由发送路径按 filefd_ 写出（见 HttpServer/TcpConnection SendFile）。
 */

#include <string>
#include <utility>
#include <map>

class HttpResponse {
public:
    enum HttpStatusCode {
        kUnkonwn = 1,
        k100Continue = 100,
        k200K = 200,
        k301K = 301,
        k302K = 302,
        k400BadRequest = 400,
        k403Forbidden = 403,
        k404NotFound = 404,
        k500internalServerError = 500,
        k503ServiceUnavailable = 503
    };

    enum HttpBodyType { HTML_TYPE, FILE_TYPE };

    /** @param close_connection 是否在响应后关闭连接 */
    explicit HttpResponse(bool close_connection);
    ~HttpResponse();

    void SetStatusCode(HttpStatusCode status_code);
    void SetStatusMessage(const std::string &status_message);
    void SetCloseConnection(bool close_connection);

    void SetContentType(const std::string &content_type);
    void SetContentLength(const int &len);
    int GetContentLength();
    void AddHeader(const std::string &key, const std::string &value);
    void SetBody(const std::string &body);

    /** 完整响应字节（状态行+头+body），内存 body 时使用 */
    std::string message();
    /** 仅状态行+头（文件下载时先发头部再发文件内容） */
    std::string beforebody();

    bool IsCloseConnection();

    int filefd() const;
    HttpBodyType bodytype() const;
    void SetFileFd(int filefd);
    void SetBodyType(HttpBodyType bodytype);

private:
    std::map<std::string, std::string> headers_;
    HttpStatusCode status_code_;
    std::string status_message_;
    int content_length_;
    std::string body_;
    bool close_connection_;
    int filefd_;
    HttpBodyType body_type_;
};
