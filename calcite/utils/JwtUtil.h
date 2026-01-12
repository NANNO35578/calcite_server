#pragma once

#include <string>
#include <map>
#include <chrono>

namespace calcite {
namespace utils {

class JwtUtil {
public:
    // 生成 JWT token
    static std::string generateToken(int64_t userId, const std::string& username, int expireHours = 24 * 7);
    
    // 验证并解析 JWT token
    static bool verifyToken(const std::string& token, int64_t& userId, std::string& username);
    
    // 从 token 中提取用户 ID（不验证，仅解析）
    static bool parseToken(const std::string& token, int64_t& userId, std::string& username);

private:
    static const std::string SECRET_KEY;
    static std::string base64Encode(const std::string& data);
    static std::string base64Decode(const std::string& data);
    static std::string hmacSha256(const std::string& data, const std::string& key);
};

} // namespace utils
} // namespace calcite

