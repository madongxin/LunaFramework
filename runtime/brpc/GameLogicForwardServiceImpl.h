#pragma once


/**
 * @file GameLogicForwardServiceImpl.h
 * @brief GameLogic Forward：跨进程转发已鉴权游戏帧
 *
 * 不验密；依赖上游已绑定 SessionHandle。
 */
#include "forward.pb.h"

/** GameLogic 侧：接收 Gateway Forward，串行队列上执行 HandleFrame */
class GameLogicForwardServiceImpl : public fwd::GameLogicForward {
public:
    void Forward(::google::protobuf::RpcController *controller, const ::fwd::ForwardReq *request,
                 ::fwd::ForwardRsp *response, ::google::protobuf::Closure *done) override;
};
