#pragma once

/**
 * @file ProtoFraming.h
 * @brief 游戏 TCP 帧格式：4 字节大端长度前缀 + protobuf payload
 *
 * 线格式：
 *   [ uint32_t len 网络字节序大端 ][ len 字节的 GameRequest/GameResponse 体 ]
 *
 * Gateway 流式缓冲循环 Decode/TryDecode；非法长度或超限应关连接。
 * 运行时上限见 MaxFrameSize()（LUNASERVER_MAX_FRAME_BYTES，夹在 [1KiB, kAbsoluteMax]）。
 */

#include <cstdint>
#include <string>

namespace gameproto {

/** 默认单帧 payload 上限 4MB。运行时可用 LUNASERVER_MAX_FRAME_BYTES 覆盖。 */
constexpr uint32_t kMaxFrameSize = 4 * 1024 * 1024;
/** 配置不得超过该编译期上限，避免误把帧上限放到过大。 */
constexpr uint32_t kAbsoluteMaxFrameSize = 16 * 1024 * 1024;

/** 当前进程生效的单帧 payload 上限（首次调用时读环境变量并夹紧）。 */
uint32_t MaxFrameSize();

enum class FrameDecodeResult {
    Complete,    // 取出一帧并消费缓冲
    Incomplete,  // 半包，保留数据等待后续字节
    Invalid      // 非法长度 / 超限，调用方应关闭连接
};

/**
 * 将 protobuf 序列化结果加上 4 字节长度头，写入 *out。
 * payload 超过 MaxFrameSize() 或 out 为空时返回 false。
 */
bool EncodeFrame(const std::string &payload, std::string *out);

/**
 * 从 buffer[offset] 起解析一帧，不修改 buffer。
 * Complete 时 *payload 填好，*consumed 为 4+len；Incomplete/Invalid 不消费。
 */
FrameDecodeResult DecodeOneFrameFrom(const std::string &buffer, size_t offset, std::string *payload,
                                     size_t *consumed);

/**
 * 从 *buffer 头部解析一帧；Complete 时 erase 已消费字节。
 * Incomplete 不改缓冲；Invalid 时调用方应关连（可不消费）。
 */
FrameDecodeResult DecodeOneFrame(std::string *buffer, std::string *payload);

/**
 * 兼容旧接口：仅 Complete 返回 true。
 * Incomplete 与 Invalid 均 false——调用方若需区分半包与坏帧，请用 DecodeOneFrame。
 */
bool TryDecodeOneFrame(std::string *buffer, std::string *payload);

}  // namespace gameproto
