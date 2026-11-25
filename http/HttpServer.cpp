/**
 * @file HttpServer.cpp
 * @brief HTTP 管理口实现：解析请求 → 访问控制 → 业务回调 → 回包
 *
 * 鉴权顺序：X-LunaServer-Token / Bearer / Cookie / ?access_token
 * 另见 LUNASERVER_HTTP_MONITOR_OPEN 对观察页路径放行。
 */

#include "HttpServer.h"
#include "HttpResponse.h"
#include "HttpRequest.h"
#include "HttpContext.h"
#include "Acceptor.h"
#include "TcpServer.h"
#include "TcpConnection.h"
#include "Buffer.h"
#include "EventLoop.h"
#include "Logging.h"
#include "TimeStamp.h"
#include "LunaServerPaths.h"
#include "SafePath.h"
#include "FormalMode.h"
#include "HttpAdminAccess.h"
#include <cstdlib>
#include <arpa/inet.h>
#include <functional>
#include <iostream>
#include <fstream>
#include <unistd.h>

/** 未注册业务回调时返回 404 */
void HttpServer::HttpDefaultCallBack(const HttpRequest &request, HttpResponse *resp) {
    (void)request;
    resp->SetStatusCode(HttpResponse::HttpStatusCode::k404NotFound);
    resp->SetStatusMessage("Not Found");
    resp->SetCloseConnection(true);
}

HttpServer::HttpServer(EventLoop *loop, const char *ip, const int port, bool auto_close_conn)
    : loop_(loop), auto_close_conn_(auto_close_conn) {
    server_ = std::make_unique<TcpServer>(loop_, ip, port);
    server_->set_connection_callback(
        std::bind(&HttpServer::onConnection, this, std::placeholders::_1));
    server_->set_message_callback(
        std::bind(&HttpServer::onMessage, this, std::placeholders::_1));
    SetHttpCallback(std::bind(&HttpServer::HttpDefaultCallBack, this, std::placeholders::_1,
                              std::placeholders::_2));
    LOG_INFO << "HttpServer Listening on [ " << ip << ":" << port << " ]";
}

HttpServer::~HttpServer() = default;

/** 新连接：记录对端；可选挂空闲超时关闭 */
void HttpServer::onConnection(const TcpConnectionPtr &conn) {
    int clnt_fd = conn->fd();
    struct sockaddr_in peeraddr;
    socklen_t peer_addrlength = sizeof(peeraddr);
    getpeername(clnt_fd, (struct sockaddr *)&peeraddr, &peer_addrlength);

    LOG_INFO << "HttpServer::OnNewConnection : Add connection "
             << "[ fd#" << clnt_fd << "-id#" << conn->id() << " ]"
             << " from " << inet_ntoa(peeraddr.sin_addr) << ":" << ntohs(peeraddr.sin_port);

    if (auto_close_conn_) {
        loop_->RunAfter(AUTOCLOSETIMEOUT,
                        std::bind(&HttpServer::ActiveCloseConn, this,
                                  std::weak_ptr<TcpConnection>(conn)));
    }
}

/** 可读：增量解析；凑齐一请求后进入 onRequest */
void HttpServer::onMessage(const TcpConnectionPtr &conn) {
    if (conn->state() != TcpConnection::ConnectionState::Connected)
        return;

    if (auto_close_conn_)
        conn->UpdateTimeStamp(TimeStamp::Now());

    HttpContext *context = conn->context();
    if (conn->read_buf()->readablebytes() <= 0)
        return;
    if (!context->ParaseRequest(conn->read_buf()->RetrieveAllAsString())) {
        LOG_INFO << "HttpServer::onMessage : Receive non HTTP message";
        conn->Send("HTTP/1.1 400 Bad Request\r\n\r\n");
        conn->HandleClose();
        return;
    }

    if (context->GetCompleteRequest()) {
        onRequest(conn, *context->request());
        context->ResetContextStatus();
    }
}

void HttpServer::SetHttpCallback(const HttpServer::HttpResponseCallback &cb) {
    response_callback_ = std::move(cb);
}

/**
 * 完整请求处理：鉴权 →（可选）multipart 上传 → 业务回调 → Send。
 * 失败鉴权直接 403 并关连接。
 */
void HttpServer::onRequest(const TcpConnectionPtr &conn, const HttpRequest &request) {
    std::string peer = "127.0.0.1";
    struct sockaddr_in peeraddr;
    socklen_t peer_len = sizeof(peeraddr);
    if (getpeername(conn->fd(), (struct sockaddr *)&peeraddr, &peer_len) == 0)
        peer = inet_ntoa(peeraddr.sin_addr);

    // ----- 收集调用方出示的管理令牌 -----
    const char *configured_env = std::getenv("LUNASERVER_HTTP_ACCESS_TOKEN");
    const std::string configured = configured_env ? configured_env : "";
    std::string presented = request.GetHeader("X-LunaServer-Token");
    if (presented.empty()) {
        const std::string auth = request.GetHeader("Authorization");
        if (auth.size() > 7 && auth.compare(0, 7, "Bearer ") == 0)
            presented = auth.substr(7);
    }
    if (presented.empty())
        presented = HttpCookieValue(request.GetHeader("Cookie"), "lunaserver_token");
    // 解析器把 ? 后面的参数放进 request_params，url() 里没有查询串。
    std::string query_token = HttpQueryParam(request.url(), "access_token");
    if (query_token.empty()) {
        const auto &params = request.request_params();
        const auto it = params.find("access_token");
        if (it != params.end())
            query_token = it->second;
    }
    if (presented.empty())
        presented = query_token;

    // 观察页路径可在 MONITOR_OPEN 下免令牌；GM 等仍走 EvaluateHttpAdminAccess
    const bool monitor_open =
        HttpMonitorOpenFromEnv() && IsHttpMonitorPublicPath(request.url());
    const HttpAdminAccessDecision access =
        monitor_open ? HttpAdminAccessDecision{true, "monitor_open"}
                     : EvaluateHttpAdminAccess(FormalModeEnabled(), peer, configured, presented);
    if (!access.allow) {
        LOG_WARN << "HttpServer refused admin request from " << peer << " reason=" << access.reason;
        conn->Send("HTTP/1.1 403 Forbidden\r\nContent-Length: 10\r\nConnection: close\r\n\r\nforbidden\n");
        conn->HandleClose();
        return;
    }
    const bool set_access_cookie = !configured.empty() && query_token == configured;

    // 日志脱敏
    std::string logged_url = request.url();
    const size_t secret = logged_url.find("access_token=");
    if (secret != std::string::npos) {
        const size_t amp = logged_url.find('&', secret);
        logged_url.replace(secret, amp == std::string::npos ? std::string::npos : amp - secret,
                           "access_token=redacted");
    }
    LOG_INFO << "HttpServer::onMessage - Request URL : " << logged_url << " from TcpConnection"
             << "[ fd#" << conn->fd() << "-id#" << conn->id() << " ]";

    std::string connection_state = request.GetHeader("Connection");
    bool isclose = (connection_state == "Close" ||
                    (request.version() == HttpRequest::Version::kHttp10 &&
                     connection_state != "keep-alive"));

    // multipart 上传：落盘到 files/（SafeBasename / SafeJoinUnderRoot 防穿越）
    if (request.GetHeader("Content-Type").find("multipart/form-data") != std::string::npos) {
        size_t boundary_index = request.GetHeader("Content-Type").find("boundary");
        std::string boundary =
            request.GetHeader("Content-Type").substr(boundary_index + std::string("boundary=").size());

        std::string filemessage = request.body();
        size_t begin_index = filemessage.find("filename");
        if (begin_index == std::string::npos) {
            LOG_ERROR << "cant find filename";
            return;
        }
        begin_index += std::string("filename=\"").size();
        size_t end_index = filemessage.find("\"\r\n", begin_index);

        std::string filename = filemessage.substr(begin_index, end_index - begin_index);
        std::string safe_name;
        if (!SafeBasename(filename, &safe_name)) {
            LOG_ERROR << "rejected upload filename";
            return;
        }
        const size_t slash = safe_name.find_last_of('/');
        filename = slash == std::string::npos ? safe_name : safe_name.substr(slash + 1);

        begin_index = filemessage.find("\r\n\r\n") + 4;
        end_index = filemessage.find(std::string("--") + boundary + "--");

        std::string filedata = filemessage.substr(begin_index, end_index - begin_index);
        std::string upload_path;
        if (!SafeJoinUnderRoot(LunaServerPaths::FilesRoot(), filename, &upload_path)) {
            LOG_ERROR << "rejected upload path";
            return;
        }
        std::ofstream ofs(upload_path, std::ios::out | std::ios::app | std::ios::binary);
        ofs.write(filedata.data(), filedata.size());
        ofs.close();
    }

    HttpResponse response(isclose);
    response_callback_(request, &response);
    if (set_access_cookie) {
        response.AddHeader("Set-Cookie",
                           "lunaserver_token=" + configured + "; Path=/; HttpOnly; SameSite=Lax");
    }

    if (response.bodytype() == HttpResponse::HttpBodyType::HTML_TYPE) {
        conn->Send(response.message());
    } else {
        // 文件：先发头部再 SendFile
        conn->Send(response.beforebody());
        conn->SendFile(response.filefd(), response.GetContentLength());
        int ret = ::close(response.filefd());
        if (ret == -1)
            LOG_ERROR << "Close File Error";
        else
            LOG_INFO << "Close File Ok";
    }

    if (response.IsCloseConnection())
        conn->HandleClose();
}

/** 拉起子 IO 线程；主 Loop 由外层 EventLoop::Loop 驱动 */
void HttpServer::Start() { server_->Start(); }

void HttpServer::SetThreadNums(int thread_nums) { server_->SetThreadNums(thread_nums); }

/** 空闲超时：超时则关连接，否则再挂一轮定时器 */
void HttpServer::ActiveCloseConn(std::weak_ptr<TcpConnection> &connection) {
    TcpConnectionPtr conn = connection.lock();
    if (!conn)
        return;
    if (TimeStamp::AddTime(conn->timestamp(), AUTOCLOSETIMEOUT) < TimeStamp::Now()) {
        conn->HandleClose();
    } else {
        loop_->RunAfter(AUTOCLOSETIMEOUT,
                        std::bind(&HttpServer::ActiveCloseConn, this, connection));
    }
}
