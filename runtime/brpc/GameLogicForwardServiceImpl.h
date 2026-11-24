#pragma once


/**
 * GameLogicForwardServiceImpl.h — Gateway→Logic Forward 实现。
 * 路径: runtime/brpc/GameLogicForwardServiceImpl.h
 */
#include "forward.pb.h"

/** GameLogic 侧：接收 Gateway Forward，串行队列上执行 HandleFrame */
class GameLogicForwardServiceImpl : public fwd::GameLogicForward {
public:
    void Forward(::google::protobuf::RpcController *controller, const ::fwd::ForwardReq *request,
                 ::fwd::ForwardRsp *response, ::google::protobuf::Closure *done) override;
};
