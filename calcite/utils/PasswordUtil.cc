#include "PasswordUtil.h"
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <sstream>
#include <iomanip>
#include <cstring>

namespace calcite {
namespace utils {

std::string PasswordUtil::hashPassword(const std::string& password) {
    // 使用 OpenSSL 的 EVP 进行 SHA-256 哈希
    // 注意：生产环境建议使用 bcrypt，这里使用 SHA-256 + salt 作为简化实现
    unsigned char salt[16];
    RAND_bytes(salt, sizeof(salt));
    
    unsigned char hash[32];
    EVP_MD_CTX* ctx = EVP_MD_CTX_new();
    EVP_DigestInit_ex(ctx, EVP_sha256(), nullptr);
    EVP_DigestUpdate(ctx, salt, sizeof(salt));
    EVP_DigestUpdate(ctx, password.c_str(), password.length());
    EVP_DigestFinal_ex(ctx, hash, nullptr);
    EVP_MD_CTX_free(ctx);
    
    // 将 salt 和 hash 组合存储
    std::ostringstream oss;
    oss << std::hex;
    for (int i = 0; i < 16; i++) {
        oss << std::setw(2) << std::setfill('0') << (int)salt[i];
    }
    for (int i = 0; i < 32; i++) {
        oss << std::setw(2) << std::setfill('0') << (int)hash[i];
    }
    return oss.str();
}

bool PasswordUtil::verifyPassword(const std::string& password, const std::string& hash) {
    if (hash.length() != 96) { // 16 bytes salt + 32 bytes hash = 48 hex chars * 2 = 96
        return false;
    }
    
    // 提取 salt
    unsigned char salt[16];
    for (int i = 0; i < 16; i++) {
        std::string hexByte = hash.substr(i * 2, 2);
        salt[i] = static_cast<unsigned char>(std::stoul(hexByte, nullptr, 16));
    }
    
    // 计算密码哈希
    unsigned char computedHash[32];
    EVP_MD_CTX* ctx = EVP_MD_CTX_new();
    EVP_DigestInit_ex(ctx, EVP_sha256(), nullptr);
    EVP_DigestUpdate(ctx, salt, sizeof(salt));
    EVP_DigestUpdate(ctx, password.c_str(), password.length());
    EVP_DigestFinal_ex(ctx, computedHash, nullptr);
    EVP_MD_CTX_free(ctx);
    
    // 提取存储的 hash
    unsigned char storedHash[32];
    for (int i = 0; i < 32; i++) {
        std::string hexByte = hash.substr(32 + i * 2, 2);
        storedHash[i] = static_cast<unsigned char>(std::stoul(hexByte, nullptr, 16));
    }
    
    // 比较
    return memcmp(computedHash, storedHash, 32) == 0;
}

} // namespace utils
} // namespace calcite

