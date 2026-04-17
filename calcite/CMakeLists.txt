cmake_minimum_required(VERSION 3.5)
project(calcite CXX)

include(CheckIncludeFileCXX)

check_include_file_cxx(any HAS_ANY)
check_include_file_cxx(string_view HAS_STRING_VIEW)
check_include_file_cxx(coroutine HAS_COROUTINE)
if (NOT "${CMAKE_CXX_STANDARD}" STREQUAL "")
elseif (HAS_ANY AND HAS_STRING_VIEW AND HAS_COROUTINE)
    set(CMAKE_CXX_STANDARD 20)
elseif (HAS_ANY AND HAS_STRING_VIEW)
    set(CMAKE_CXX_STANDARD 17)
else ()
    set(CMAKE_CXX_STANDARD 14)
endif ()

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g")
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# 把所有业务代码编译成静态库
aux_source_directory(controllers CTL_SRC)
aux_source_directory(filters FILTER_SRC)
aux_source_directory(plugins PLUGIN_SRC)
aux_source_directory(models MODEL_SRC)
aux_source_directory(services SERVICE_SRC)
aux_source_directory(utils UTIL_SRC)

add_library(calcite_lib STATIC
    ${CTL_SRC}
    ${FILTER_SRC}
    ${PLUGIN_SRC}
    ${MODEL_SRC}
    ${SERVICE_SRC}
    ${UTIL_SRC}
)

# 主程序
add_executable(${PROJECT_NAME} main.cc)

# 链接 Drogon
find_package(Drogon CONFIG REQUIRED)
target_link_libraries(calcite_lib PRIVATE Drogon::Drogon)
target_link_libraries(${PROJECT_NAME} PRIVATE calcite_lib Drogon::Drogon)

# 链接 CURL、OpenSSL
find_package(CURL REQUIRED)
find_package(OpenSSL REQUIRED)
target_link_libraries(calcite_lib PRIVATE CURL::libcurl OpenSSL::SSL OpenSSL::Crypto)
target_link_libraries(${PROJECT_NAME} PRIVATE CURL::libcurl OpenSSL::SSL OpenSSL::Crypto)

# C++ 版本检查
if (CMAKE_CXX_STANDARD LESS 17)
    message(FATAL_ERROR "c++17 or higher is required")
elseif (CMAKE_CXX_STANDARD LESS 20)
    message(STATUS "use c++17")
else ()
    message(STATUS "use c++20")
endif ()

# 创建视图
drogon_create_views(${PROJECT_NAME} ${CMAKE_CURRENT_SOURCE_DIR}/views ${CMAKE_CURRENT_BINARY_DIR})

# 头文件路径
target_include_directories(calcite_lib
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/models
)
target_include_directories(${PROJECT_NAME}
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/models
)

add_subdirectory(test)