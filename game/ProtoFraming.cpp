/**
 * @file ProtoFraming.cpp
 * @brief 长度前缀帧编解码与 MaxFrameSize 环境覆盖
 */

#include "ProtoFraming.h"

#include <arpa/inet.h>
#include <cstdlib>
#include <cstring>

namespace gameproto {

/** 读 LUNASERVER_MAX_FRAME_BYTES，夹在 [1024, kAbsoluteMaxFrameSize]。 */
uint32_t MaxFrameSize() {
    static const uint32_t kLimit = [] {
        const char *env = std::getenv("LUNASERVER_MAX_FRAME_BYTES");
        unsigned long n = kMaxFrameSize;
        if (env && *env)
            n = std::strtoul(env, nullptr, 10);
        if (n < 1024)
            n = 1024;
        if (n > kAbsoluteMaxFrameSize)
            n = kAbsoluteMaxFrameSize;
        return static_cast<uint32_t>(n);
    }();
    return kLimit;
}

/** 写入 4 字节大端长度 + payload；超限返回 false。 */
bool EncodeFrame(const std::string &payload, std::string *out) {
    if (!out || payload.size() > MaxFrameSize())
        return false;
    uint32_t be = htonl(static_cast<uint32_t>(payload.size()));
    out->assign(reinterpret_cast<const char *>(&be), sizeof(be));
    out->append(payload);
    return true;
}

/** 非破坏性解析：Complete 时 *consumed=4+len；半包 Incomplete；非法 Invalid。 */
FrameDecodeResult DecodeOneFrameFrom(const std::string &buffer, size_t offset, std::string *payload,
                                     size_t *consumed) {
    if (!payload)
        return FrameDecodeResult::Invalid;
    if (buffer.size() < offset + 4)
        return FrameDecodeResult::Incomplete;

    uint32_t be = 0;
    std::memcpy(&be, buffer.data() + offset, 4);
    const uint32_t len = ntohl(be);

    if (len == 0 || len > MaxFrameSize())
        return FrameDecodeResult::Invalid;

    if (buffer.size() < offset + 4u + len)
        return FrameDecodeResult::Incomplete;

    payload->assign(buffer.data() + offset + 4, len);
    if (consumed)
        *consumed = 4u + len;
    return FrameDecodeResult::Complete;
}

/** 调用 DecodeOneFrameFrom(0) 成功则 erase 消费字节。 */
FrameDecodeResult DecodeOneFrame(std::string *buffer, std::string *payload) {
    if (!buffer)
        return FrameDecodeResult::Invalid;
    size_t consumed = 0;
    const FrameDecodeResult dr = DecodeOneFrameFrom(*buffer, 0, payload, &consumed);
    if (dr == FrameDecodeResult::Complete)
        buffer->erase(0, consumed);
    return dr;
}

/** 兼容旧接口：仅 Complete→true。 */
bool TryDecodeOneFrame(std::string *buffer, std::string *payload) {
    return DecodeOneFrame(buffer, payload) == FrameDecodeResult::Complete;
}

}  // namespace gameproto
