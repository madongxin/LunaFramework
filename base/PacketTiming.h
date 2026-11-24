#pragma once

/**
 * @file PacketTiming.h
 * @brief 按包种记录处理/RPC/回包排队耗时与发送水位
 *
 * 慢包写日志；AOI/Move 跳过原因（重传、未发送字节过多）单独计数。
 */

#include <cstdint>
#include <string>

/** 记录某包某阶段耗时；超阈值写慢包日志 */
void NotePktStage(const char *kind, const char *stage, int64_t ms, uint64_t conn, uint64_t player,
                  uint64_t seq, int app_bytes, int unsent_bytes);

/** 更新应用缓冲与内核未发送字节水位 */
void NoteSendPending(int app_bytes, int unsent_bytes);

/** Move 合帧：是否新调度一轮 flush */
void NoteMoveOffer(bool scheduled_new);
/** Move flush 结果：是否真正发出、实体数 */
void NoteMoveFlush(bool sent, int entities);
/** 内核未发送字节还多，最新移动帧只留在应用缓冲，没有 write。 */
void NoteMoveHold(int entities);
/** 这一拍没有把位置写进内核：连接正在重传。 */
void NoteAoiSkipRetrans();
/** 这一拍没有把位置写进内核：内核里还没发出去的字节超过 16KB。 */
void NoteAoiSkipUnsent();
/** 在途降到 16KB 以内，写出了这一帧位置。 */
void NoteAoiWriteRoom();
/** 位置已经停够 1 秒，强制写出这一帧。 */
void NoteAoiWriteStarved();
/** 当前待合帧的移动实体数 */
void NoteMovePendingEntities(int n);

/** 打出上一个 10 秒窗口最慢包与发送水位，并刷新监控最大值 */
void PacketTimingLogWindow();

/** 导出本模块 Prometheus 文本片段 */
std::string PacketTimingPrometheus();
