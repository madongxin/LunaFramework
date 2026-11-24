#pragma once

/**
 * @file AoiMembership.h
 * @brief 统一兴趣集成员判定开关（进入半径 / 离开半径 + 固定 cap）
 *
 * LUNASERVER_AOI_MEMBERSHIP_UNIFIED=1 时 Move/RefreshViewers/sync_pose/SyncVisibleSubject
 * 共用同一套规则；默认关闭。人数上限 AoiUnifiedCap()=200。
 */

#include <cstdlib>

/**
 * 默认关。打开后 Move、RefreshViewers、sync_pose、SyncVisibleSubject
 * 用同一套判定：进入半径内才新加入，已在视野里的人留到离开半径，
 * 人数上限固定 200，名次滞回用 AoiV2CapSlack。
 */
inline bool AoiMembershipUnified() {
    const char *env = std::getenv("LUNASERVER_AOI_MEMBERSHIP_UNIFIED");
    return env && env[0] == '1' && env[1] == '\0';
}

inline int AoiUnifiedCap() { return 200; }
