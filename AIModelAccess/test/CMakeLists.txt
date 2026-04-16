#设置Cmake的最小版本
cmake_minimum_required(VERSION 3.10)

#设置项目名称
project(AIModelAccessTest)

#设置C++标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

#设置构建类型Debug
set(CMAKE_BUILD_TYPE Debug)

#添加可执行文件
add_executable(AIModelAccessTest testLLM.cpp ../sdk/src/DeepSeekProvider.cpp ../sdk/src/util/myLog.cpp)

#设置输出目录
set(EXECUTABLE_OUTPUT_PATH ${CMAKE_BINARY_DIR})

#添加头文件搜索路径
include_directories(${CMAKE_PROJECT_INCLUDE_DIR}/../sdk/include)

#查找OpenSSL库find_package(OpenSSL REQUIRED)
find_package(OpenSSL REQUIRED)


#添加宏定义，启用httplib库的HTTPS支持
target_compile_definitions(AIModelAccessTest PRIVATE CPPHTTPLIB_OPENSSL_SUPPORT)

#链接库
target_link_libraries(AIModelAccessTest jsoncpp spdlog fmt gtest OpenSSL::SSL OpenSSL::Crypto pthread)