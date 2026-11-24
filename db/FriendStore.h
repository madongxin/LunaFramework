#pragma once

/**
 * @file FriendStore.h
 * @brief 好友关系 MySQL 仓储（仅 GameDB 进程访问）
 *
 * Execute 按 FriendOpReq 分发 List/Request/Accept/Reject 等事务。
 */

#include "gamedb.pb.h"

#include <string>

class FriendStore {
public:
    static FriendStore &Instance();

    /** 建表（若不存在） */
    bool EnsureTables();
    void Execute(const gdb::FriendOpReq &req, gdb::FriendOpRsp *rsp);
    /** 把已过期的 PENDING 标成 EXPIRED，单轮最多 limit 行。 */
    int ExpireStaleRequests(int limit);

private:
    FriendStore() = default;

    void ListFriends(const gdb::FriendOpReq &req, gdb::FriendOpRsp *rsp);
    void Search(const gdb::FriendOpReq &req, gdb::FriendOpRsp *rsp);
    void Apply(const gdb::FriendOpReq &req, gdb::FriendOpRsp *rsp);
    void RequestList(const gdb::FriendOpReq &req, gdb::FriendOpRsp *rsp);
    void Accept(const gdb::FriendOpReq &req, gdb::FriendOpRsp *rsp);
    void Reject(const gdb::FriendOpReq &req, gdb::FriendOpRsp *rsp);
    void DeleteFriend(const gdb::FriendOpReq &req, gdb::FriendOpRsp *rsp);
    void Block(const gdb::FriendOpReq &req, gdb::FriendOpRsp *rsp);
    void BlockGate(const gdb::FriendOpReq &req, gdb::FriendOpRsp *rsp);
    void Unblock(const gdb::FriendOpReq &req, gdb::FriendOpRsp *rsp);
    void BlockList(const gdb::FriendOpReq &req, gdb::FriendOpRsp *rsp);
};
