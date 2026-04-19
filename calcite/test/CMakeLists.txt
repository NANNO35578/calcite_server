cmake_minimum_required(VERSION 3.5)
project(calcite_test CXX)

# Clear any inherited variables from parent scope
unset(CTL_SRC)
unset(FILTER_SRC)
unset(PLUGIN_SRC)
unset(MODEL_SRC)
unset(SERVICE_SRC)
unset(UTIL_SRC)

# Collect source files from parent directories
aux_source_directory(../controllers CTL_SRC)
aux_source_directory(../filters FILTER_SRC)
aux_source_directory(../plugins PLUGIN_SRC)
aux_source_directory(../models MODEL_SRC)
aux_source_directory(../services SERVICE_SRC)
aux_source_directory(../utils UTIL_SRC)

# Add test executable
add_executable(${PROJECT_NAME}
               test_main.cc
               ${CTL_SRC}
               ${FILTER_SRC}
               ${PLUGIN_SRC}
               ${MODEL_SRC}
               ${SERVICE_SRC}
               ${UTIL_SRC})

# Link libraries (already found by parent CMakeLists.txt)
target_link_libraries(${PROJECT_NAME}
                      PRIVATE Drogon::Drogon
                              CURL::libcurl
                              OpenSSL::SSL
                              OpenSSL::Crypto)

# Include directories
target_include_directories(${PROJECT_NAME}
                           PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/..
                                   ${CMAKE_CURRENT_SOURCE_DIR}/../models)
