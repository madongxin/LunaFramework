#pragma once


/**
 * SecureRandom.h — 密码学安全随机（token/fence）。
 * 路径: runtime/SecureRandom.h
 */
#include <cstddef>
#include <cstdint>
#include <string>

/** 密码学安全随机数；失败必须 fail-closed，禁止回退 mt19937/rand/time/pid。 */
namespace SecureRandom {

bool Fill(void *buf, size_t n);
bool Hex(size_t nchars, std::string *out);

}  // namespace SecureRandom
