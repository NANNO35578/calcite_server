#include "AuthService.h"
#include "../utils/PasswordUtil.h"
#include "../utils/JwtUtil.h"
#include <drogon/drogon.h>
#include <iostream>

namespace calcite {
namespace services {

AuthService::AuthService()
    : userMapper_(drogon::app().getDbClient("default")) {}

void AuthService::registerUser(const std::string& username,
                              const std::string& email,
                              const std::string& password,
                              std::function<void(const RegisterResult&)> callback) {
    // 检查用户名是否已存在
    userMapper_.findBy(
        drogon::orm::Criteria(drogon_model::calcite::User::Cols::_username, drogon::orm::CompareOperator::EQ, username),
        [this, username, email, password, callback](const std::vector<drogon_model::calcite::User>& users) {
            if (!users.empty()) {
                RegisterResult result;
                result.success = false;
                result.message = "用户名已存在";
                callback(result);
                return;
            }

            // 检查邮箱是否已存在（如果提供了邮箱）
            if (!email.empty()) {
                userMapper_.findBy(
                    drogon::orm::Criteria(drogon_model::calcite::User::Cols::_email, drogon::orm::CompareOperator::EQ, email),
                    [this, username, email, password, callback](const std::vector<drogon_model::calcite::User>& users2) {
                        if (!users2.empty()) {
                            RegisterResult result;
                            result.success = false;
                            result.message = "邮箱已被注册";
                            callback(result);
                            return;
                        }

                        // 加密密码并创建用户
                        std::string passwordHash = calcite::utils::PasswordUtil::hashPassword(password);
                        drogon_model::calcite::User newUser;
                        newUser.setUsername(username);
                        newUser.setPasswordHash(passwordHash);
                        newUser.setEmail(email);

                        userMapper_.insert(
                            newUser,
                            [callback](const drogon_model::calcite::User& insertedUser) {
                                RegisterResult result;
                                result.success = true;
                                result.message = "注册成功";
                                result.userId = insertedUser.getValueOfId();
                                callback(result);
                            },
                            [callback](const drogon::orm::DrogonDbException& e) {
                                RegisterResult result;
                                result.success = false;
                                result.message = "注册失败: " + std::string(e.base().what());
                                callback(result);
                            });
                    },
                    [callback](const drogon::orm::DrogonDbException& e) {
                        RegisterResult result;
                        result.success = false;
                        result.message = "检查邮箱失败: " + std::string(e.base().what());
                        callback(result);
                    });
            } else {
                // 没有邮箱，直接注册
                std::string passwordHash = calcite::utils::PasswordUtil::hashPassword(password);
                drogon_model::calcite::User newUser;
                newUser.setUsername(username);
                newUser.setPasswordHash(passwordHash);

                userMapper_.insert(
                    newUser,
                    [callback](const drogon_model::calcite::User& insertedUser) {
                        RegisterResult result;
                        result.success = true;
                        result.message = "注册成功";
                        result.userId = insertedUser.getValueOfId();
                        callback(result);
                    },
                    [callback](const drogon::orm::DrogonDbException& e) {
                        RegisterResult result;
                        result.success = false;
                        result.message = "注册失败: " + std::string(e.base().what());
                        callback(result);
                    });
            }
        },
        [callback](const drogon::orm::DrogonDbException& e) {
            RegisterResult result;
            result.success = false;
            result.message = "检查用户名失败: " + std::string(e.base().what());
            callback(result);
        });
}

void AuthService::loginUser(const std::string& username,
                           const std::string& password,
                           std::function<void(const LoginResult&)> callback) {
    // 查询用户
    userMapper_.findBy(
        drogon::orm::Criteria(drogon_model::calcite::User::Cols::_username, drogon::orm::CompareOperator::EQ, username),
        [password, callback](const std::vector<drogon_model::calcite::User>& users) {
            LoginResult result;
            if (users.empty()) {
                result.success = false;
                result.message = "用户名或密码错误";
                callback(result);
                return;
            }

            const auto& user = users[0];
            std::string passwordHash = user.getValueOfPasswordHash();

            // 验证密码
            if (!calcite::utils::PasswordUtil::verifyPassword(password, passwordHash)) {
                result.success = false;
                result.message = "用户名或密码错误";
                callback(result);
                return;
            }

            // 生成 token (JWT 模式，无需落库)
            std::string token = calcite::utils::JwtUtil::generateToken(
                user.getValueOfId(),
                user.getValueOfUsername());

            result.success = true;
            result.message = "登录成功";
            result.token = token;
            result.userId = user.getValueOfId();
            result.username = user.getValueOfUsername();
            callback(result);
        },
        [callback](const drogon::orm::DrogonDbException& e) {
            LoginResult result;
            result.success = false;
            result.message = "登录失败: " + std::string(e.base().what());
            callback(result);
        });
}

void AuthService::logoutUser(const std::string& token,
                            std::function<void(bool, const std::string&)> callback) {
    // JWT 模式下，退出登录只需客户端删除 token，服务器无需操作
    (void)token;
    callback(true, "退出登录成功");
}

void AuthService::verifyToken(const std::string& token,
                             std::function<void(bool, int64_t, const std::string&)> callback) {
    // JWT 模式下，直接验证 JWT 即可
    int64_t userId = 0;
    std::string username;
    bool valid = calcite::utils::JwtUtil::verifyToken(token, userId, username);
    callback(valid, userId, username);
}

} // namespace services
} // namespace calcite
