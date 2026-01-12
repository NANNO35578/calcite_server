#pragma once

#include <string>

namespace calcite {
namespace utils {

class PasswordUtil {
public:
    // 加密密码（使用 bcrypt）
    static std::string hashPassword(const std::string& password);
    
    // 验证密码
    static bool verifyPassword(const std::string& password, const std::string& hash);
};

} // namespace utils
} // namespace calcite

