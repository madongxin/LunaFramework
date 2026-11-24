/**
 * @file MailTypes.cpp
 * @brief 邮件可见态/附件态合法转换
 */

#include "MailTypes.h"

namespace mail {

/** 仅 ACTIVE 可转到 SOFT_DELETED/EXPIRED/REVOKED；同态幂等。 */
bool CanTransitVisible(VisibleState from, VisibleState to) {
    if (from == to)
        return true;
    if (from != VisibleState::kActive)
        return false;
    return to == VisibleState::kSoftDeleted || to == VisibleState::kExpired ||
           to == VisibleState::kRevoked;
}

/** 主路径 NONE→UNCLAIMED→CLAIMING→CLAIMED；同态幂等。 */
bool CanTransitAttachment(AttachmentState from, AttachmentState to) {
    if (from == to)
        return true;
    if (from == AttachmentState::kNone && to == AttachmentState::kUnclaimed)
        return true;
    if (from == AttachmentState::kUnclaimed && to == AttachmentState::kClaiming)
        return true;
    if (from == AttachmentState::kClaiming &&
        (to == AttachmentState::kClaimed || to == AttachmentState::kUnclaimed))
        return true;
    // 预留扩展路径
    if (from == AttachmentState::kUnclaimed && to == AttachmentState::kDestroyed)
        return true;
    if (from == AttachmentState::kClaimed && to == AttachmentState::kReturned)
        return true;
    return false;
}

}  // namespace mail
