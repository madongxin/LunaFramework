#pragma once


/**
 * AoiPushSendRoute.h — AOI delta 走合并路径还是急送路径的判定。
 * pure MOVE 必须 coalesce；ENTER/LEAVE 走 urgent，避免被 MOVE 堵住。
 * 路径: runtime/AoiPushSendRoute.h
 */
#include <string>

/**
 * GameLogic marks pure MOVE as coalescable=true (reliable=false).
 * Those frames must use bind.send_frame → OfferCoalescedMove → Pick/EGRESS_ENFORCE.
 * ENTER/LEAVE (or mixed) stay on send_frame_urgent so they are not blocked behind MOVE.
 *
 * Historical bug: GatewayPush sent every aoi.delta.v1 via urgent, so move_offer /
 * admitted / budget_defer stayed 0 while tx_frames{move} still grew.
 */
inline bool AoiDeltaUsesCoalescePath(const std::string &message_type, bool coalescable) {
    return message_type == "aoi.delta.v1" && coalescable;
}
