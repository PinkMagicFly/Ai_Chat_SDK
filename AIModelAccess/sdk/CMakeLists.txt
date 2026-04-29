#CMake最低版本要求
cmake_minimum_required(VERSION 3.10)

#设置SDK的名称
set(SDK_NAME "ai_chat_SDK")

#设置C++标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

#设置构建类型
set(CMAKE_BUILD_TYPE Release)

#收集源文件
file(GLOB_RECURSE SDK_SOURCES "src/*.cpp")
file(GLOB_RECURSE SDK_HEADERS "include/*.h")

#生成静态库
add_library(${SDK_NAME} STATIC ${SDK_SOURCES} ${SDK_HEADERS})

#设置输出目录
set_target_properties(${SDK_NAME} PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/build
)

#添加头文件搜索路径
target_include_directories(${SDK_NAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

#添加宏定义，启用httplib库的HTTPS支持
target_compile_definitions(${SDK_NAME} PUBLIC CPPHTTPLIB_OPENSSL_SUPPORT)

#设置静态库的目录
link_directories(/usr/local/lib)

#查找OpenSSL库find_package(OpenSSL REQUIRED)
find_package(OpenSSL REQUIRED)

#链接依赖库
target_link_libraries(${SDK_NAME} jsoncpp fmt spdlog sqlite3 OpenSSL::SSL OpenSSL::Crypto)

#安装规则 拷贝静态库文件到指定目录 /usr/local/lib
install(TARGETS ${SDK_NAME}
        ARCHIVE DESTINATION lib)

#安装规则 拷贝头文件到指定目录 /usr/local/include
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/include/ 
                    DESTINATION include/${SDK_NAME}
                    FILES_MATCHING PATTERN "*.h")