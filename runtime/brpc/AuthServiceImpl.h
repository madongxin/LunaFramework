#pragma once

/**
 * @file AuthServiceImpl.h
 * @brief AuthService brpc：Login/VerifyToken；不创建 Session
 *
 * 凭证校验经 GameDB；access token 存 Redis（AuthTokenStore）。Gateway 编排再调 Session。
 */
#include "auth.pb.h"

class AuthServiceImpl : public auth::AuthService {
public:
    /** 校验账号口令并发 access token；不创建 Session（交由 Gateway→Session）。 */
    void Login(::google::protobuf::RpcController *controller, const ::auth::LoginRequest *request,
               ::auth::LoginResponse *response, ::google::protobuf::Closure *done) override;
    void Register(::google::protobuf::RpcController *controller,
                  const ::auth::RegisterRequest *request, ::auth::RegisterResponse *response,
                  ::google::protobuf::Closure *done) override;
    /** 校验 access token 是否有效；仍不创建/修改 Session。 */
    void VerifyToken(::google::protobuf::RpcController *controller,
                     const ::auth::VerifyTokenRequest *request,
                     ::auth::VerifyTokenResponse *response,
                     ::google::protobuf::Closure *done) override;
    void RefreshToken(::google::protobuf::RpcController *controller,
                      const ::auth::RefreshTokenRequest *request,
                      ::auth::RefreshTokenResponse *response,
                      ::google::protobuf::Closure *done) override;
};

/** Auth 所用 GameDB 通道可达（Lookup/Load 无写副作用） */
bool AuthGameDbReachable(int timeout_ms = 800);
