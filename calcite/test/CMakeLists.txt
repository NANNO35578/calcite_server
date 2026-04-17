cmake_minimum_required(VERSION 3.5)
project(calcite_test CXX)

add_executable(${PROJECT_NAME} test_main.cc)

# 链接静态库 + Drogon + 依赖
target_link_libraries(${PROJECT_NAME}
    PRIVATE
    calcite_lib
    Drogon::Drogon
    CURL::libcurl
    OpenSSL::SSL
    OpenSSL::Crypto
)

ParseAndAddDrogonTests(${PROJECT_NAME})