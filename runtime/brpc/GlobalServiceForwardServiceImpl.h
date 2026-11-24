#pragma once


/**
 * @file GlobalServiceForwardServiceImpl.h
 * @brief GlobalService Forward：邮件与社交等全局请求处理
 *
 * 邮件持久化最终落 GameDB；Formal 下会话故障不放行（见 MailDevSkipSession）。
 */
#include "forward.pb.h"

/** GlobalService Forward：邮件/全局业务入队 HandleFrame；无地图 epoch 栅栏、无战斗 Tick */
class GlobalServiceForwardServiceImpl : public fwd::GlobalServiceForward {
public:
    void Forward(::google::protobuf::RpcController *controller, const ::fwd::ForwardReq *request,
                 ::fwd::ForwardRsp *response, ::google::protobuf::Closure *done) override;
};
