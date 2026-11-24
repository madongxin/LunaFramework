#pragma once


/**
 * AoiLeaveReason.h — AOI LEAVE 原因枚举。
 * 路径: runtime/AoiLeaveReason.h
 */
/** LEAVE 的原因。指标名 lunaserver_aoi_leave_total{reason=...}。 */
enum class AoiLeaveReason {
    OutOfWindow = 0,
    CapEvict,
    Disconnect,
    RemoveInterest,
    RetryResend,
    ReconnectResync,
    Other,
    Count
};

inline const char *AoiLeaveReasonName(AoiLeaveReason reason) {
    switch (reason) {
    case AoiLeaveReason::OutOfWindow:
        return "out_of_window";
    case AoiLeaveReason::CapEvict:
        return "cap_evict";
    case AoiLeaveReason::Disconnect:
        return "disconnect";
    case AoiLeaveReason::RemoveInterest:
        return "remove_interest";
    case AoiLeaveReason::RetryResend:
        return "retry_resend";
    case AoiLeaveReason::ReconnectResync:
        return "reconnect_resync";
    case AoiLeaveReason::Other:
        return "other";
    case AoiLeaveReason::Count:
        break;
    }
    return "other";
}
