#pragma once

#include <drogon/orm/Mapper.h>
#include "../models/User.h"
#include <string>
#include <functional>

namespace calcite {
namespace services {

struct RegisterResult {
    bool success;
    std::string message;
    int64_t userId;
};

struct LoginResult {
    bool success;
    std::string message;
    std::string token;
    int64_t userId;
    std::string username;
};

class AuthService {
public:
    AuthService();

    // 用户注册
    void registerUser(const std::string& username,
                     const std::string& email,
                     const std::string& password,
                     std::function<void(const RegisterResult&)> callback);

    // 用户登录
    void loginUser(const std::string& username,
                  const std::string& password,
                  std::function<void(const LoginResult&)> callback);

    // 退出登录（JWT 模式下仅删除客户端 token）
    void logoutUser(const std::string& token,
                   std::function<void(bool, const std::string&)> callback);

    // 验证 token 并获取用户信息
    void verifyToken(const std::string& token,
                    std::function<void(bool, int64_t, const std::string&)> callback);

private:
    drogon::orm::Mapper<drogon_model::calcite::User> userMapper_;
};

} // namespace services
} // namespace calcite

