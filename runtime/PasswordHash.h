#pragma once

/**
 * @file PasswordHash.h
 * @brief PBKDF2-HMAC-SHA256 口令哈希与校验（OpenSSL）
 *
 * 默认 100000 次迭代、16 字节盐、32 字节摘要。日志中禁止打印明文或完整 hash，
 * 可用 RedactSecret 脱敏。Sha256Hex 另供 token 摘要 / 幂等键材料。
 *
 * 协作：PasswordExec 卸载；Auth/GameDB 存 salt+iterations+hash。
 */

#include <string>

/** PBKDF2-HMAC-SHA256 密码哈希（OpenSSL）；日志中禁止打印明文或完整 hash。 */
namespace PasswordHash {

constexpr int kDefaultIterations = 100000;
constexpr size_t kSaltBytes = 16;
constexpr size_t kHashBytes = 32;

/** 生成随机盐并写为小写 hex。 */
bool GenerateSalt(std::string *salt_hex);
/**
 * 计算密码哈希。
 * @param salt_hex 小写 hex 盐；iterations 建议 >= kDefaultIterations
 * @param hash_hex 输出小写 hex 摘要
 */
bool HashPassword(const std::string &password, const std::string &salt_hex, int iterations,
                  std::string *hash_hex);
/**
 * 校验密码是否匹配 expected_hash_hex（常量时间比较语义由实现保证）。
 */
bool VerifyPassword(const std::string &password, const std::string &salt_hex, int iterations,
                    const std::string &expected_hash_hex);

/** 脱敏：仅保留前后各 2 字符，中间以 * 代替。 */
std::string RedactSecret(const std::string &s);

/** SHA-256 hex（小写），用于 token 摘要 / 幂等键材料。 */
bool Sha256Hex(const std::string &data, std::string *hex_out);

}  // namespace PasswordHash
