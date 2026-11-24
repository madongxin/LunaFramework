#pragma once

/**
 * @file FriendService.h
 * @brief 好友业务：列表/申请/拉黑/私聊门禁与在线态扇出
 *
 * 正式集群多在 GlobalService/World 侧启用；GameLogic 可通过
 * SetFriendCommandsEnabled(false) 关闭本进程好友命令。
 * 持久化经 GameDB；在线态/推送目标旁路 SessionStore；
 * FanoutPresence 异步推好友上线/下线，停服前须 StopPresenceFanout。
 */

#include "game.pb.h"

#include <cstdint>
#include <string>

/**
 * 私聊是否可以投递。
 * Hide：对方拉黑了自己——对发送方返回成功，但不把消息送出去（防探测）。
 */
enum class FriendWhisperGate { Deliver, Reject, Hide };

class FriendService {
public:
    static FriendService &Instance();

    /** 好友列表（可带存在感摘要）。 */
    bool HandleList(const game::FriendListReq &req, game::GameResponse *rsp);
    /** 按名/关键字搜可加好友的玩家。 */
    bool HandleSearch(const game::FriendSearchReq &req, game::GameResponse *rsp);
    /** 发起好友申请（受 SessionStore 申请限流）。 */
    bool HandleApply(const game::FriendApplyReq &req, game::GameResponse *rsp);
    /** 待处理申请列表。 */
    bool HandleRequestList(const game::FriendRequestListReq &req, game::GameResponse *rsp);
    /** 接受申请：双边建友并刷新旁路好友缓存。 */
    bool HandleAccept(const game::FriendAcceptReq &req, game::GameResponse *rsp);
    /** 拒绝申请。 */
    bool HandleReject(const game::FriendRejectReq &req, game::GameResponse *rsp);
    /** 删除好友关系并更新旁路缓存。 */
    bool HandleDelete(const game::FriendDeleteReq &req, game::GameResponse *rsp);
    /** 拉黑：解除好友关系并写入 block 缓存。 */
    bool HandleBlock(const game::FriendBlockReq &req, game::GameResponse *rsp);
    /** 取消拉黑并失效 block 缓存。 */
    bool HandleUnblock(const game::FriendUnblockReq &req, game::GameResponse *rsp);
    /** 拉黑列表。 */
    bool HandleBlockList(const game::FriendBlockListReq &req, game::GameResponse *rsp);

    /**
     * 玩家上线/下线时向在线好友扇出 presence。
     * 按 gateway 分批（FriendPresenceBatch），经 Push 定向到各 Gateway。
     */
    void FanoutPresence(uint64_t player_id, bool online);
    /** 停服前停掉 presence 工作线程，避免退出后还访问 Redis/brpc。 */
    void StopPresenceFanout();
    /**
     * 私聊门禁：非好友 Reject；被拉黑 Hide；通过则 Deliver。
     * *error_code 仅在 Reject 时填写（Hide 对外仍成功）。
     */
    FriendWhisperGate GateWhisper(uint64_t actor_player_id, uint64_t target_player_id,
                                  std::string *error_code);

private:
    FriendService() = default;
};
