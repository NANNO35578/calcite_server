#include "AuthService.h"
#include "../utils/PasswordUtil.h"
#include "../utils/JwtUtil.h"
#include <drogon/drogon.h>
#include <json/json.h>
#include <iostream>
#include <functional>
#include <trantor/utils/Date.h>

namespace calcite {
namespace services {

AuthService::AuthService() {
    dbClient_ = drogon::app().getDbClient("default");
}

void AuthService::registerUser(const std::string& username,
                              const std::string& email,
                              const std::string& password,
                              std::function<void(const RegisterResult&)> callback) {
    RegisterResult result;
    
    // 检查用户名是否已存在
    std::string checkSql = "SELECT id FROM user WHERE username = ?";
    dbClient_->execSqlAsync(
        checkSql,
        [this, username, email, password, callback, result](const drogon::orm::Result& r) mutable {
            RegisterResult newResult = result;
            if (!r.empty()) {
                newResult.success = false;
                newResult.message = "用户名已存在";
                callback(newResult);
                return;
            }
            
            // 检查邮箱是否已存在（如果提供了邮箱）
            if (!email.empty()) {
                std::string checkEmailSql = "SELECT id FROM user WHERE email = ?";
                dbClient_->execSqlAsync(
                    checkEmailSql,
                    [this, username, email, password, callback, newResult](const drogon::orm::Result& r2) mutable {
                        RegisterResult finalResult = newResult;
                        if (!r2.empty()) {
                            finalResult.success = false;
                            finalResult.message = "邮箱已被注册";
                            callback(finalResult);
                            return;
                        }
                        
                        // 加密密码
                        std::string passwordHash = calcite::utils::PasswordUtil::hashPassword(password);
                        
                        // 插入新用户
                        std::string insertSql = "INSERT INTO user (username, email, password_hash) VALUES (?, ?, ?)";
                        dbClient_->execSqlAsync(
                            insertSql,
                            [callback](const drogon::orm::Result& r3) {
                                RegisterResult result;
                                result.success = true;
                                result.message = "注册成功";
                                result.userId = r3.insertId();
                                callback(result);
                            },
                            [callback](const drogon::orm::DrogonDbException& e) {
                                RegisterResult result;
                                result.success = false;
                                result.message = "注册失败: " + std::string(e.base().what());
                                callback(result);
                            },
                            username, email, passwordHash);
                    },
                    [callback](const drogon::orm::DrogonDbException& e) {
                        RegisterResult result;
                        result.success = false;
                        result.message = "检查邮箱失败: " + std::string(e.base().what());
                        callback(result);
                    },
                    email);
            } else {
                // 没有邮箱，直接注册
                std::string passwordHash = calcite::utils::PasswordUtil::hashPassword(password);
                std::string insertSql = "INSERT INTO user (username, password_hash) VALUES (?, ?)";
                dbClient_->execSqlAsync(
                    insertSql,
                    [callback](const drogon::orm::Result& r3) {
                        RegisterResult result;
                        result.success = true;
                        result.message = "注册成功";
                        result.userId = r3.insertId();
                        callback(result);
                    },
                    [callback](const drogon::orm::DrogonDbException& e) {
                        RegisterResult result;
                        result.success = false;
                        result.message = "注册失败: " + std::string(e.base().what());
                        callback(result);
                    },
                    username, passwordHash);
            }
        },
        [callback](const drogon::orm::DrogonDbException& e) {
            RegisterResult result;
            result.success = false;
            result.message = "检查用户名失败: " + std::string(e.base().what());
            callback(result);
        },
        username);
}

void AuthService::loginUser(const std::string& username,
                           const std::string& password,
                           std::function<void(const LoginResult&)> callback) {
    LoginResult result;
    
    // 查询用户
    std::string sql = "SELECT id, username, password_hash FROM user WHERE username = ?";
    dbClient_->execSqlAsync(
        sql,
        [password, callback](const drogon::orm::Result& r) {
            LoginResult result;
            if (r.empty()) {
                result.success = false;
                result.message = "用户名或密码错误";
                callback(result);
                return;
            }
            
            int64_t userId = r[0]["id"].as<int64_t>();
            std::string username = r[0]["username"].as<std::string>();
            std::string passwordHash = r[0]["password_hash"].as<std::string>();
            
            // 验证密码
            if (!calcite::utils::PasswordUtil::verifyPassword(password, passwordHash)) {
                result.success = false;
                result.message = "用户名或密码错误";
                callback(result);
                return;
            }
            
            // 生成 token
            std::string token = calcite::utils::JwtUtil::generateToken(userId, username);
            
            // 保存 token 到数据库
            auto dbClient = drogon::app().getDbClient("default");
            auto now = trantor::Date::date();
            auto expiredAt = now.after(7 * 24 * 3600); // 7天后过期
            
            std::string insertTokenSql = "INSERT INTO user_token (user_id, token, expired_at) VALUES (?, ?, ?)";
            dbClient->execSqlAsync(
                insertTokenSql,
                [token, userId, username, callback](const drogon::orm::Result&) {
                    LoginResult result;
                    result.success = true;
                    result.message = "登录成功";
                    result.token = token;
                    result.userId = userId;
                    result.username = username;
                    callback(result);
                },
                [callback](const drogon::orm::DrogonDbException& e) {
                    LoginResult result;
                    result.success = false;
                    result.message = "保存token失败: " + std::string(e.base().what());
                    callback(result);
                },
                userId, token, expiredAt.toDbStringLocal());
        },
        [callback](const drogon::orm::DrogonDbException& e) {
            LoginResult result;
            result.success = false;
            result.message = "登录失败: " + std::string(e.base().what());
            callback(result);
        },
        username);
}

void AuthService::logoutUser(const std::string& token,
                            std::function<void(bool, const std::string&)> callback) {
    std::string sql = "DELETE FROM user_token WHERE token = ?";
    dbClient_->execSqlAsync(
        sql,
        [callback](const drogon::orm::Result& r) {
            callback(true, "退出登录成功");
        },
        [callback](const drogon::orm::DrogonDbException& e) {
            callback(false, "退出登录失败: " + std::string(e.base().what()));
        },
        token);
}

void AuthService::verifyToken(const std::string& token,
                             std::function<void(bool, int64_t, const std::string&)> callback) {
    // 先验证 JWT token
    int64_t userId;
    std::string username;
    if (!calcite::utils::JwtUtil::verifyToken(token, userId, username)) {
        callback(false, 0, "");
        return;
    }
    
    // 检查 token 是否在数据库中（可选，用于支持 token 撤销）
    std::string sql = "SELECT user_id FROM user_token WHERE token = ? AND expired_at > NOW()";
    dbClient_->execSqlAsync(
        sql,
        [userId, username, callback](const drogon::orm::Result& r) {
            if (r.empty()) {
                callback(false, 0, "");
            } else {
                callback(true, userId, username);
            }
        },
        [userId, username, callback](const drogon::orm::DrogonDbException& e) {
            // 即使数据库查询失败，如果 JWT 验证成功，也认为有效
            callback(true, userId, username);
        },
        token);
}

void AuthService::saveToken(int64_t userId, const std::string& token, std::function<void()> callback) {
    auto now = trantor::Date::date();
    auto expiredAt = now.after(7 * 24 * 3600);
    
    std::string sql = "INSERT INTO user_token (user_id, token, expired_at) VALUES (?, ?, ?)";
    dbClient_->execSqlAsync(
        sql,
        [callback](const drogon::orm::Result&) {
            callback();
        },
        [callback](const drogon::orm::DrogonDbException& e) {
            callback();
        },
        userId, token, expiredAt.toDbStringLocal());
}

} // namespace services
} // namespace calcite

