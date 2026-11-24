#pragma once

/**
 * @file MailBrpcServiceImpl.h
 * @brief brpc MailBrpcService 实现：鉴权后转发到 MailService / SessionStore
 *
 * 每个 RPC：校验 fence/token（或内部投递 token）→ 填 GameRequest → MailService::Handle*
 * → 抽出对应 *Rsp。MailDeliver 另走 LUNASERVER_MAIL_DELIVER_TOKEN 常量时间比较。
 */

#include "mail_brpc.pb.h"

class MailBrpcServiceImpl : public mailrpc::MailBrpcService {
public:
    /** 调试/旧路径 Login；正式集群会话应走 Session 服务而非本接口。 */
    void Login(::google::protobuf::RpcController *controller, const ::game::LoginReq *request,
               ::game::LoginRsp *response, ::google::protobuf::Closure *done) override;

    void MailboxSummary(::google::protobuf::RpcController *controller,
                        const ::mailrpc::AuthedMailboxSummaryReq *request,
                        ::game::MailboxSummaryRsp *response,
                        ::google::protobuf::Closure *done) override;

    void MailList(::google::protobuf::RpcController *controller,
                  const ::mailrpc::AuthedMailListReq *request, ::game::MailListRsp *response,
                  ::google::protobuf::Closure *done) override;

    void MailGet(::google::protobuf::RpcController *controller,
                 const ::mailrpc::AuthedMailGetReq *request, ::game::MailGetRsp *response,
                 ::google::protobuf::Closure *done) override;

    void MailRead(::google::protobuf::RpcController *controller,
                  const ::mailrpc::AuthedMailReadReq *request, ::game::MailReadRsp *response,
                  ::google::protobuf::Closure *done) override;

    void MailClaim(::google::protobuf::RpcController *controller,
                   const ::mailrpc::AuthedMailClaimReq *request, ::game::MailClaimRsp *response,
                   ::google::protobuf::Closure *done) override;

    void MailBatchClaim(::google::protobuf::RpcController *controller,
                        const ::mailrpc::AuthedMailBatchClaimReq *request,
                        ::game::MailBatchClaimRsp *response,
                        ::google::protobuf::Closure *done) override;

    void MailFavorite(::google::protobuf::RpcController *controller,
                      const ::mailrpc::AuthedMailFavoriteReq *request,
                      ::game::MailFavoriteRsp *response,
                      ::google::protobuf::Closure *done) override;

    void MailBatchRead(::google::protobuf::RpcController *controller,
                       const ::mailrpc::AuthedMailBatchReadReq *request,
                       ::game::MailBatchReadRsp *response,
                       ::google::protobuf::Closure *done) override;

    void MailBatchDelete(::google::protobuf::RpcController *controller,
                         const ::mailrpc::AuthedMailBatchDeleteReq *request,
                         ::game::MailBatchDeleteRsp *response,
                         ::google::protobuf::Closure *done) override;

    /** 系统投递：须带内部 deliver token；不走玩家 fence。 */
    void MailDeliver(::google::protobuf::RpcController *controller,
                     const ::mailrpc::AuthedMailDeliverReq *request,
                     ::game::MailDeliverRsp *response, ::google::protobuf::Closure *done) override;
};
