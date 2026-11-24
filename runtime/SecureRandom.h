#pragma once

/**
 * @file SecureRandom.h
 * @brief 密码学安全随机字节（token/盐/fence 材料）
 *
 * 禁止用 rand()；失败时调用方必须中止发证。
 */

#include <cstddef>
#include <cstdint>
#include <string>

/** 密码学安全随机数；失败必须 fail-closed，禁止回退 mt19937/rand/time/pid。 */
namespace SecureRandom {

bool Fill(void *buf, size_t n);
bool Hex(size_t nchars, std::string *out);

}  // namespace SecureRandom
