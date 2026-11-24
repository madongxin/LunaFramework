/**
 * @file TcpServer.cpp
 * @brief TcpServer：accept 在主 reactor，连接 IO 在子 EventLoop
 *
 * 关闭路径必须「主线程删 map → 子线程 ConnectionDestructor」，
 * 避免与 HandleNewConnection 并发改 connectionsMap_。
 */

#include "TcpServer.h"
#include "Acceptor.h"
#include "EventLoop.h"
#include "EventLoopThreadPool.h"
#include "Logging.h"
#include "TcpConnection.h"
#include <cassert>

TcpServer::TcpServer(EventLoop *loop, const char *ip, const int port)
    : main_reactor_(loop), next_conn_id_(1) {
  // listen 挂在主 loop；新连接回调到 HandleNewConnection
  acceptor_ = std::make_unique<Acceptor>(main_reactor_, ip, port);
  acceptor_->set_newconnection_callback(
      std::bind(&TcpServer::HandleNewConnection, this, std::placeholders::_1));
  thread_pool_ = std::make_unique<EventLoopThreadPool>(loop);
}

TcpServer::~TcpServer() {}

/** 先起 IO 线程池，再阻塞进入主 Loop（只 accept） */
void TcpServer::Start() {
  thread_pool_->start();
  main_reactor_->Loop();
}

/**
 * accept 得到的新 fd：分配单调 conn_id，挂到某个子 reactor，
 * 再在该线程 ConnectionEstablished（注册 epoll + on_connect_）。
 */
void TcpServer::HandleNewConnection(int fd) {
  assert(fd != -1);

  EventLoop *sub_reactor = thread_pool_->nextloop();

  const uint64_t conn_id = next_conn_id_++;
  auto conn = std::make_shared<TcpConnection>(sub_reactor, fd, conn_id);
  conn->set_connection_callback(on_connect_);
  conn->set_close_callback(
      std::bind(&TcpServer::HandleClose, this, std::placeholders::_1));
  conn->set_message_callback(on_message_);

  connectionsMap_[conn_id] = conn;

  // Channel 只在所属 IO 线程加入 epoll；与 HandleMessage/析构同线程串行。
  sub_reactor->RunOneFunc([conn]() { conn->ConnectionEstablished(); });
}

/** 子线程关闭回调入口：立刻转主线程删表，避免锁 map */
void TcpServer::HandleClose(const std::shared_ptr<TcpConnection> &conn) {
  main_reactor_->RunOneFunc(
      std::bind(&TcpServer::HandleCloseInLoop, this, conn));
}

/** 主线程：业务 disconnect 回调 → erase map → 子线程析构 Channel */
void TcpServer::HandleCloseInLoop(const std::shared_ptr<TcpConnection> &conn) {
  LOG_INFO << "TcpServer::HandleCloseInLoop - Remove connection [id#"
           << conn->id() << "-fd#" << conn->fd() << "]";
  if (on_disconnect_)
    on_disconnect_(conn);
  auto it = connectionsMap_.find(conn->id());
  if (it != connectionsMap_.end())
    connectionsMap_.erase(it);

  conn->loop()->QueueOneFunc(
      std::bind(&TcpConnection::ConnectionDestructor, conn));
}

void TcpServer::set_connection_callback(
    std::function<void(const std::shared_ptr<TcpConnection> &)> const &fn) {
  on_connect_ = std::move(fn);
}

void TcpServer::set_message_callback(
    std::function<void(const std::shared_ptr<TcpConnection> &)> const &fn) {
  on_message_ = std::move(fn);
}

void TcpServer::set_disconnect_callback(
    std::function<void(const std::shared_ptr<TcpConnection> &)> const &fn) {
  on_disconnect_ = std::move(fn);
}

/** 优雅停服：不再 accept（已有连接仍由子 reactor 处理） */
void TcpServer::StopAccepting() {
  if (acceptor_)
    acceptor_->PauseListening();
}

void TcpServer::SetThreadNums(int thread_nums) {
  thread_pool_->SetThreadNums(thread_nums);
}
