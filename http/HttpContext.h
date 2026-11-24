#pragma once

/**
 * @file HttpContext.h
 * @brief 按连接挂载的 HTTP 请求解析状态机
 *
 * 每次 onMessage 喂入增量字节；GetCompleteRequest() 为真后再取 request()。
 * ResetContextStatus() 在处理完一请求后调用，以支持 keep-alive 下一请求。
 */

#include <string>
#include <memory>

class HttpRequest;

#define CR '\r'
#define LF '\n'

/** 解析器状态（行/头/body） */
enum HttpRequestParaseState {
    kINVALID,
    kINVALID_METHOD,
    kINVALID_URL,
    kINVALID_VERSION,
    kINVALID_HEADER,

    START,
    METHOD,

    BEFORE_URL,
    IN_URL,

    BEFORE_URL_PARAM_KEY,
    URL_PARAM_KEY,
    BEFORE_URL_PARAM_VALUE,
    URL_PARAM_VALUE,

    BEFORE_PROTOCOL,
    PROTOCOL,

    BEFORE_VERSION,
    VERSION_SPLIT,
    VERSION,

    HEADER,
    HEADER_KEY,

    HEADER_BEFORE_COLON,
    HEADER_AFTER_COLON,
    HEADER_VALUE,

    WHEN_CR,
    CR_LF,
    CR_LF_CR,

    BODY = 100,
    COMPLETE = 101,
};

class HttpContext {
public:
    HttpContext();
    ~HttpContext();

    /** 喂入一段原始字节，推进状态机；返回是否仍合法 */
    bool ParaseRequest(const char *begin, int size);
    bool ParaseRequest(const std::string &msg);
    bool ParaseRequest(const char *begin);

    /** 当前是否已凑齐完整请求（可进入业务回调） */
    bool GetCompleteRequest();
    /** 解析结果；生命周期随 Context */
    HttpRequest *request();
    /** 重置状态与 request，准备解析下一请求 */
    void ResetContextStatus();

private:
    std::unique_ptr<HttpRequest> request_;
    HttpRequestParaseState state_;
};
