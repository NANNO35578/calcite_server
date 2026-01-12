#include "JwtUtil.h"
#include <openssl/hmac.h>
#include <openssl/sha.h>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <cctype>
#include <chrono>

namespace calcite {
namespace utils {

const std::string JwtUtil::SECRET_KEY = "calcite_secret_key_2024";

// 简单的 Base64 编码（简化实现）
static const char base64_chars[] = 
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

std::string JwtUtil::base64Encode(const std::string& data) {
    std::string ret;
    int i = 0;
    int j = 0;
    unsigned char char_array_3[3];
    unsigned char char_array_4[4];
    
    size_t in_len = data.size();
    const unsigned char* bytes_to_encode = reinterpret_cast<const unsigned char*>(data.c_str());
    
    while (in_len--) {
        char_array_3[i++] = *(bytes_to_encode++);
        if (i == 3) {
            char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
            char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
            char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);
            char_array_4[3] = char_array_3[2] & 0x3f;
            
            for (i = 0; i < 4; i++)
                ret += base64_chars[char_array_4[i]];
            i = 0;
        }
    }
    
    if (i) {
        for (j = i; j < 3; j++)
            char_array_3[j] = '\0';
        
        char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
        char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
        char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);
        char_array_4[3] = char_array_3[2] & 0x3f;
        
        for (j = 0; j < i + 1; j++)
            ret += base64_chars[char_array_4[j]];
        
        while (i++ < 3)
            ret += '=';
    }
    
    return ret;
}

std::string JwtUtil::base64Decode(const std::string& encoded_string) {
    int in_len = encoded_string.size();
    int i = 0;
    int j = 0;
    int in = 0;
    unsigned char char_array_4[4], char_array_3[3];
    std::string ret;
    
    while (in_len-- && (encoded_string[in] != '=') && 
           (isalnum(encoded_string[in]) || (encoded_string[in] == '+') || (encoded_string[in] == '/'))) {
        char_array_4[i++] = encoded_string[in]; in++;
        if (i == 4) {
            for (i = 0; i < 4; i++)
                char_array_4[i] = static_cast<unsigned char>(std::string(base64_chars).find(char_array_4[i]));
            
            char_array_3[0] = (char_array_4[0] << 2) + ((char_array_4[1] & 0x30) >> 4);
            char_array_3[1] = ((char_array_4[1] & 0xf) << 4) + ((char_array_4[2] & 0x3c) >> 2);
            char_array_3[2] = ((char_array_4[2] & 0x3) << 6) + char_array_4[3];
            
            for (i = 0; (i < 3); i++)
                ret += char_array_3[i];
            i = 0;
        }
    }
    
    if (i) {
        for (j = i; j < 4; j++)
            char_array_4[j] = 0;
        
        for (j = 0; j < 4; j++)
            char_array_4[j] = static_cast<unsigned char>(std::string(base64_chars).find(char_array_4[j]));
        
        char_array_3[0] = (char_array_4[0] << 2) + ((char_array_4[1] & 0x30) >> 4);
        char_array_3[1] = ((char_array_4[1] & 0xf) << 4) + ((char_array_4[2] & 0x3c) >> 2);
        char_array_3[2] = ((char_array_4[2] & 0x3) << 6) + char_array_4[3];
        
        for (j = 0; (j < i - 1); j++) ret += char_array_3[j];
    }
    
    return ret;
}

std::string JwtUtil::hmacSha256(const std::string& data, const std::string& key) {
    unsigned char* digest = HMAC(EVP_sha256(), key.c_str(), key.length(),
                                 reinterpret_cast<const unsigned char*>(data.c_str()),
                                 data.length(), nullptr, nullptr);
    
    std::ostringstream oss;
    oss << std::hex;
    for (int i = 0; i < 32; i++) {
        oss << std::setw(2) << std::setfill('0') << static_cast<int>(digest[i]);
    }
    return oss.str();
}

std::string JwtUtil::generateToken(int64_t userId, const std::string& username, int expireHours) {
    // Header
    std::string header = R"({"alg":"HS256","typ":"JWT"})";
    std::string encodedHeader = base64Encode(header);
    
    // Payload
    auto now = std::chrono::system_clock::now();
    auto exp = now + std::chrono::hours(expireHours);
    auto expTime = std::chrono::duration_cast<std::chrono::seconds>(exp.time_since_epoch()).count();
    
    std::ostringstream payloadStream;
    payloadStream << R"({"user_id":)" << userId 
                  << R"(,"username":")" << username 
                  << R"(,"exp":)" << expTime << "}";
    std::string payload = payloadStream.str();
    std::string encodedPayload = base64Encode(payload);
    
    // Signature
    std::string data = encodedHeader + "." + encodedPayload;
    std::string signature = hmacSha256(data, SECRET_KEY);
    std::string encodedSignature = base64Encode(signature);
    
    return data + "." + encodedSignature;
}

bool JwtUtil::verifyToken(const std::string& token, int64_t& userId, std::string& username) {
    size_t dot1 = token.find('.');
    size_t dot2 = token.find('.', dot1 + 1);
    
    if (dot1 == std::string::npos || dot2 == std::string::npos) {
        return false;
    }
    
    std::string encodedHeader = token.substr(0, dot1);
    std::string encodedPayload = token.substr(dot1 + 1, dot2 - dot1 - 1);
    std::string encodedSignature = token.substr(dot2 + 1);
    
    // 验证签名
    std::string data = encodedHeader + "." + encodedPayload;
    std::string signature = hmacSha256(data, SECRET_KEY);
    std::string expectedSignature = base64Encode(signature);
    
    if (encodedSignature != expectedSignature) {
        return false;
    }
    
    // 解析 payload
    return parseToken(token, userId, username);
}

bool JwtUtil::parseToken(const std::string& token, int64_t& userId, std::string& username) {
    size_t dot1 = token.find('.');
    size_t dot2 = token.find('.', dot1 + 1);
    
    if (dot1 == std::string::npos || dot2 == std::string::npos) {
        return false;
    }
    
    std::string encodedPayload = token.substr(dot1 + 1, dot2 - dot1 - 1);
    std::string payload = base64Decode(encodedPayload);
    
    // 简单解析 JSON（生产环境建议使用 jsoncpp）
    size_t userIdPos = payload.find("\"user_id\":");
    size_t usernamePos = payload.find("\"username\":\"");
    size_t expPos = payload.find("\"exp\":");
    
    if (userIdPos == std::string::npos || usernamePos == std::string::npos || expPos == std::string::npos) {
        return false;
    }
    
    // 提取 user_id
    userIdPos += 10;
    size_t userIdEnd = payload.find_first_of(",}", userIdPos);
    userId = std::stoll(payload.substr(userIdPos, userIdEnd - userIdPos));
    
    // 提取 username
    usernamePos += 12;
    size_t usernameEnd = payload.find('"', usernamePos);
    username = payload.substr(usernamePos, usernameEnd - usernamePos);
    
    // 检查过期时间
    expPos += 6;
    size_t expEnd = payload.find_first_of(",}", expPos);
    int64_t exp = std::stoll(payload.substr(expPos, expEnd - expPos));
    auto now = std::chrono::system_clock::now();
    auto expTime = std::chrono::system_clock::time_point(std::chrono::seconds(exp));
    
    if (now > expTime) {
        return false; // Token 已过期
    }
    
    return true;
}

} // namespace utils
} // namespace calcite

