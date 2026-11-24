#pragma once

/**
 * @file GameLogic.h
 * @brief 玩法业务单例：处理已鉴权的 GameRequest（移动/AOI/邮件领取等）
 *
 * 由 GameService::HandleFrame / Gateway 转发在反序列化后调用。
 * Formal 下不接受密码/凭证；好友命令可关（正式集群好友在 World）；
 * 资产权威在 GameDB，内存背包仅缓存；AOI 推送必须在 MapRuntime 锁外。
 * BindAuthenticatedPlayer 只加载已认证身份，不得经此入口做 Login 验密。
 */

#include "game.pb.h"
#include "IGameDbRepository.h"
#include "MapRuntime.h"

#include <cstdint>
#include <map>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

/**
 * 本进程玩法状态与命令分发中心（单例）。
 * 不拥有 TCP；不创建 Session；跨 Logic 迁移只导出/导入运行时态。
 */
class GameLogic {
public:
    static GameLogic &Instance();
    /**
     * 按 oneof body 分发到各 HandleXxx，填充 GameResponse（含 seq）。
     * 需 session_token 的命令在入口 RequireSessionToken；危险命令由 CommandPolicy 先拦。
     */
    bool Handle(const game::GameRequest &req, game::GameResponse *rsp);
    /**
     * 正式集群的 GameLogic 不处理好友命令（好友在 World）。
     * 默认开启，供单进程和 World 复用同一分发。
     */
    void SetFriendCommandsEnabled(bool enabled);

    /** 邮件领取等：查询内存背包聚合数量 */
    uint32_t GetItemCount(uint64_t player_id, uint32_t item_id);
    /** 提交 GameDB 前拷贝背包快照（供 worker 软上限校验） */
    void CopyInventory(uint64_t player_id, std::unordered_map<uint32_t, uint32_t> *out);
    /** 邮件领取事务提交后同步增加内存背包 */
    bool ApplyItemReward(uint64_t player_id, uint32_t item_id, uint32_t count);
    /**
     * 邮件领取成功：同锁内应用 grants 并设为 DB 提交后的 asset_version（禁止回退）。
     * 版本无法衔接时在锁外从 GameDB 重载完整背包。
     */
    bool ApplyItemRewardsWithVersion(uint64_t player_id, const std::vector<GameDbGrantedItem> &grants,
                                     uint64_t committed_asset_version);
    /** 当前内存资产版本（与 GameDB asset_version 对齐；测试/调试可读）。 */
    uint64_t GetAssetVersion(uint64_t player_id);
    /** 内存与权威可能不一致（重载失败等）；为 true 时应先 EnsureAssetsSynced。 */
    bool IsAssetDirty(uint64_t player_id);
    /** dirty 时尝试权威重载；成功后允许资产写 */
    bool EnsureAssetsSynced(uint64_t player_id);
    /** 从 GameDB 重载背包。失败时把角色标脏。 */
    bool ReloadAuthoritativeAssets(uint64_t player_id);
    /** 单测：阻断从 GameDB 重载，模拟权威不可达。 */
    void SetAssetReloadBlockedForTest(bool v);
    /** 单测：阻断 ApplyItemReward 写内存。 */
    void SetAssetApplyBlockedForTest(bool v);
    /** 单测：重载时注入指定背包与版本，跳过真实 GameDB。 */
    void SetAssetReloadOverrideForTest(const std::map<uint32_t, uint32_t> &bag, uint64_t ver);
    void ClearAssetReloadOverrideForTest();
    /** 单测入口：绕过 Handle 分发直接测消耗。 */
    bool HandleConsumeItemForTest(const game::ConsumeItemReq &req, game::GameResponse *rsp) {
        return HandleConsumeItem(req, rsp);
    }
    bool HandleGrantItemForTest(const game::GrantItemReq &req, game::GameResponse *rsp) {
        return HandleGrantItem(req, rsp);
    }
    bool HandleMoveForTest(const game::MoveReq &req, game::GameResponse *rsp) {
        return HandleMove(req, rsp);
    }
    bool HandleRespawnForTest(const game::RespawnReq &req, game::GameResponse *rsp) {
        return HandleRespawn(req, rsp);
    }
    /** 事务失败时回退内存（尽力而为） */
    bool RollbackItemReward(uint64_t player_id, uint32_t item_id, uint32_t count);

    /**
     * Gateway BindPlayer：已认证玩家加载内存态（背包/画像/进图前置）。
     * 不接收密码或 access_token；失败 fail-closed，不得半加载放行。
     */
    bool BindAuthenticatedPlayer(uint64_t player_id, std::string *err = nullptr,
                                 game::PlayerAttributes *profile_out = nullptr);
    /** 读内存画像；未加载返回 false。 */
    bool GetPlayerAttributes(uint64_t player_id, game::PlayerAttributes *out);
    /** 将内存背包异步/同步刷到持久化队列；reason 仅用于日志。 */
    bool FlushBag(uint64_t player_id, const std::string &reason);
    /** 锁外推送 AOI（brpc/Redis 不得在 MapRuntime 锁内） */
    void EmitAoi(const AoiPushBatch &batch);
    /** 宽限到期后删掉断线实体，并再通知周围还在线的人。 */
    void SweepDisconnected();
    /** 推进客户端可见运动插值（定时器调用；与权威 Move 解耦）。 */
    void AdvancePresentedMotion();
    /** 进程退出前排空并收回 AOI 发布线程。没启动过则什么都不做。 */
    void StopAoiPublisher();
    /** 转移冻结期间暂存该玩家的 AOI，Commit/Import 后按原顺序重放。 */
    void SetAoiHold(uint64_t player_id, bool hold);

    /**
     * 跨 Logic 迁移导出：运行时背包与技能 CD（不含凭证/Session）。
     * 配合 SessionStore Begin/CommitPlayerTransfer 与对端 ImportRuntimeState。
     */
    bool ExportRuntimeState(uint64_t player_id, std::map<uint32_t, uint32_t> *bag,
                            std::map<uint32_t, int64_t> *skill_cds, uint64_t *asset_version);
    /** 迁移导入：写入本进程内存态并抬升 asset_version；随后可重放 AOI hold。 */
    bool ImportRuntimeState(uint64_t player_id, const std::map<uint32_t, uint32_t> &bag,
                            const std::map<uint32_t, int64_t> &skill_cds, uint64_t asset_version);
    /** 全量快照（Push 缺口 / WorldSnapshot）：填充 FullStateSnapshotRsp。 */
    bool BuildFullStateSnapshot(uint64_t player_id, game::FullStateSnapshotRsp *out);
    /**
     * 同上，并带 baseline_server_seq（可靠 Push 水位），供重连 NEED_SNAPSHOT 对齐。
     */
    bool BuildFullStateSnapshot(uint64_t player_id, uint64_t baseline_server_seq,
                                game::FullStateSnapshotRsp *out);
    /** 将 LastSafe 落盘/刷出（下线、切图、停服）；reason 写日志。 */
    void FlushLastSafe(uint64_t player_id, const char *reason);
    /** 进程退出或停服：刷出全部玩家 LastSafe。 */
    void FlushAllLastSafe(const char *reason);
    /** 单测：注入生命态（hp/life_state）以测复活与 AOI。 */
    void SetLifeStateForTest(uint64_t player_id, int32_t hp, const std::string &life_state);
    /** 顶号、登出、解绑时丢掉该玩家的进程内 token 缓存。 */
    void InvalidateSessionTokenCache(uint64_t player_id);

private:
    GameLogic() = default;
    bool HandleConsumeItem(const game::ConsumeItemReq &req, game::GameResponse *rsp);
    bool HandleReleaseSkill(const game::ReleaseSkillReq &req, game::GameResponse *rsp);
    /** 发放道具：内存即时生效，MySQL 经 PlayerItemPersistQueue 异步落库 */
    bool HandleGrantItem(const game::GrantItemReq &req, game::GameResponse *rsp);
    bool HandleLogin(const game::LoginReq &req, game::GameResponse *rsp);
    bool HandleReconnect(const game::ReconnectReq &req, game::GameResponse *rsp);
    bool HandleValidateSession(const game::ValidateSessionReq &req, game::GameResponse *rsp);
    bool HandleCheckOnline(const game::CheckOnlineReq &req, game::GameResponse *rsp);
    bool HandleLogout(const game::LogoutReq &req, game::GameResponse *rsp);
    bool HandleRegister(const game::RegisterReq &req, game::GameResponse *rsp);
    bool HandleFlushBag(const game::FlushBagReq &req, game::GameResponse *rsp);
    bool HandleEnterMap(const game::EnterMapReq &req, game::GameResponse *rsp);
    bool HandleLeaveMap(const game::LeaveMapReq &req, game::GameResponse *rsp);
    bool HandleMapPing(const game::MapPingReq &req, game::GameResponse *rsp);
    bool HandleChatSend(const game::ChatSendReq &req, game::GameResponse *rsp);
    bool HandleFriendList(const game::FriendListReq &req, game::GameResponse *rsp);
    bool HandleGetSelfProfile(const game::GetSelfProfileReq &req, game::GameResponse *rsp);
    bool HandleGetPlayerBrief(const game::GetPlayerBriefReq &req, game::GameResponse *rsp);
    bool HandleQueryOnlineState(const game::QueryOnlineStateReq &req, game::GameResponse *rsp);
    bool HandleMove(const game::MoveReq &req, game::GameResponse *rsp);
    bool HandleWorldSnapshot(const game::WorldSnapshotReq &req, game::GameResponse *rsp);
    bool HandleRespawn(const game::RespawnReq &req, game::GameResponse *rsp);
    bool HandleInteractPortal(const game::InteractPortalReq &req, game::GameResponse *rsp);
    bool RequireSessionToken(const game::GameRequest &req, uint64_t player_id, game::GameResponse *rsp);
    /** 不得在持有 mu_ 时调用。远程加载在锁外完成，写回内存时才加锁。 */
    bool EnsurePlayerLoaded(uint64_t player_id, std::string *err);
    /** 调用方已持有 mu_。只补内存槽，不做网络或磁盘 IO。 */
    void EnsurePlayer(uint64_t player_id);
    /** 从正式资产重载背包+版本（不得在持 mu_ 时调用远程 IO） */
    bool LoadAssetsFromGameDbUnlocked(uint64_t player_id, std::map<uint32_t, uint32_t> *bag,
                                      uint64_t *ver);
    bool TryReloadAssets(uint64_t player_id);
    bool LoadProfileUnlocked(uint64_t player_id, game::PlayerAttributes *out, std::string *err);

    std::mutex mu_;
    /** 内存背包：player_id -> (item_config_id -> 聚合数量），与 consume_item 共用 */
    std::map<uint64_t, std::map<uint32_t, uint32_t>> inventory_;
    std::map<uint64_t, std::map<uint32_t, int64_t>> skill_cd_until_ms_;
    std::map<uint64_t, uint64_t> asset_version_;
    std::map<uint64_t, bool> player_load_ok_;
    std::map<uint64_t, bool> asset_dirty_;
    std::map<uint64_t, game::PlayerAttributes> profiles_;
    struct LastSafeMem {
        uint32_t realm_id = 1;
        uint64_t map_template_id = 0;
        uint64_t map_instance_id = 0;
        float x = 0;
        float y = 0;
        float z = 0;
        float yaw = 0;
        uint64_t position_version = 0;
        int64_t last_flush_ms = 0;
        bool dirty = false;
        std::string respawn_op;
    };
    std::map<uint64_t, LastSafeMem> last_safe_;
    void MaybePersistLastSafe(uint64_t player_id, const MapEntity &e, uint64_t map_instance_id,
                              bool force);
    std::string LifeStateOf(uint64_t player_id);
    bool friend_commands_enabled_ = true;
    bool reload_blocked_for_test_ = false;
    bool apply_blocked_for_test_ = false;
    bool reload_override_for_test_ = false;
    std::map<uint32_t, uint32_t> reload_override_bag_;
    uint64_t reload_override_ver_ = 0;
};
