#CMake最低版本要求
cmake_minimum_required(VERSION 3.10)

#设置ChatServer工程名称
project(AIChatServer LANGUAGES CXX)

#设置C++标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

#设置构建类型
set(CMAKE_BUILD_TYPE Release)

#收集源文件
file(GLOB_RECURSE CHATSERVER_SOURCES "main.cpp" "ChatServer.cpp")

#生成可执行文件
add_executable(AIChatServer ${CHATSERVER_SOURCES})

#设置输出目录
set_target_properties(AIChatServer PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/build
)

#添加宏定义，启用httplib库的HTTPS支持
target_compile_definitions(AIChatServer PUBLIC CPPHTTPLIB_OPENSSL_SUPPORT)

#设置库搜索路径
link_directories(/usr/local/lib)

#查找OpenSSL库
find_package(OpenSSL REQUIRED)


#链接依赖库
target_link_libraries(AIChatServer ai_chat_SDK jsoncpp spdlog fmt gtest OpenSSL::SSL OpenSSL::Crypto pthread sqlite3 gflags)
