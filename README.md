# ChatSDK（C++）

一个基于 **C++17** 的大模型接入 SDK，统一封装多家模型 API 与本地 Ollama，提供会话管理、消息持久化、全量/流式回复能力，方便在 C++ 项目中快速接入 LLM。

## 功能特性

- 支持多模型统一接入（当前代码已实现）：
  - DeepSeek：`deepseek-chat`
  - OpenRouter(OpenAI)：`openai/gpt-4o-mini`
  - OpenRouter(Gemini)：`google/gemini-2.0-flash-001`
  - Ollama 本地模型（如 `gemma3:270m`）
- 统一 `ChatSDK` 接口调用
- 会话管理：创建、查询、列出、删除会话
- 消息发送：
  - 全量回复 `sendMessage`
  - 流式回复 `sendMessageIncremental`
- 使用 SQLite 做会话与消息持久化
- 内置日志能力（spdlog）

---

## 项目结构

```text
.
├── AIModelAccess
│   ├── sdk
│   │   ├── CMakeLists.txt
│   │   ├── include
│   │   │   ├── ChatSDK.h
│   │   │   ├── common.h
│   │   │   ├── LLMProvider.h
│   │   │   ├── DeepSeekProvider.h
│   │   │   ├── ChatGPTProvider.h
│   │   │   ├── GeminiProvider.h
│   │   │   ├── OllamaProvider.h
│   │   │   └── ...
│   │   └── src
│   └── test
├── README.md
└── README.en.md
```

---

## 环境依赖

- CMake >= 3.10
- C++17 编译器（g++ / clang++）
- 依赖库（根据 `sdk/CMakeLists.txt`）：
  - `jsoncpp`
  - `fmt`
  - `spdlog`
  - `sqlite3`
  - `OpenSSL`

> Linux 下请先安装上述开发库（如 `libssl-dev`、`libsqlite3-dev` 等）。

---

## 编译与安装 SDK

在仓库根目录执行：

```bash
cd AIModelAccess/sdk
mkdir -p build && cd build
cmake ..
make -j
sudo make install
```

默认安装位置（由 CMake install 规则决定）：

- 静态库：`/usr/local/lib`
- 头文件：`/usr/local/include/ai_chat_SDK`

---

## API 概览（核心）

头文件：

```cpp
#include <ai_chat_SDK/ChatSDK.h>
```

`ChatSDK` 常用接口：

- `bool initializeSDK(const std::vector<std::shared_ptr<Config>>& configs)`
  - 初始化并注册模型
- `std::string createSession(const std::string& modelName)`
  - 为指定模型创建会话
- `std::shared_ptr<Session> getSession(const std::string& sessionId)`
  - 获取会话对象
- `std::vector<std::string> getSessionLists() const`
  - 获取全部会话 ID
- `bool deleteSession(const std::string& sessionId)`
  - 删除会话
- `std::vector<LLMInfo> getAvailableModels() const`
  - 查询当前可用模型
- `std::string sendMessage(const std::string& sessionId, const std::string& message)`
  - 全量返回
- `std::string sendMessageIncremental(const std::string& sessionId, const std::string& message, callback)`
  - 流式返回（增量回调）

相关数据结构（`Config` / `APIConfig` / `OllamaConfig` / `Session` / `Message`）见：

- `AIModelAccess/sdk/include/common.h`

---

## 快速上手

```cpp
#include <iostream>
#include <memory>
#include <vector>
#include <ai_chat_SDK/ChatSDK.h>

int main() {
    ai_chat_sdk::ChatSDK sdk;

    // 1) 配置模型
    auto deepseek = std::make_shared<ai_chat_sdk::APIConfig>();
    deepseek->_modelName = "deepseek-chat";
    deepseek->_apiKey = std::getenv("deepseek_apikey");
    deepseek->_temperature = 0.7f;
    deepseek->_max_tokens = 2048;

    std::vector<std::shared_ptr<ai_chat_sdk::Config>> configs = {deepseek};

    // 2) 初始化
    if (!sdk.initializeSDK(configs)) {
        std::cerr << "initializeSDK failed" << std::endl;
        return 1;
    }

    // 3) 创建会话
    auto sessionId = sdk.createSession("deepseek-chat");
    if (sessionId.empty()) {
        std::cerr << "createSession failed" << std::endl;
        return 1;
    }

    // 4) 流式发送消息
    auto callback = [](const std::string& chunk, bool done) {
        std::cout << chunk;
        if (done) std::cout << "\n[done]" << std::endl;
    };

    auto full = sdk.sendMessageIncremental(sessionId, "你好，请介绍一下你自己。", callback);
    std::cout << "\nFull response: " << full << std::endl;

    return 0;
}
```

---

## 业务工程 CMake 示例

```cmake
cmake_minimum_required(VERSION 3.10)
project(AIChatDemo)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(AIChatDemo main.cpp)

find_package(OpenSSL REQUIRED)
include_directories(${OPENSSL_INCLUDE_DIR})
link_directories(/usr/local/lib)

target_link_libraries(AIChatDemo PRIVATE
    ai_chat_SDK
    fmt
    jsoncpp
    OpenSSL::SSL
    OpenSSL::Crypto
    gflags
    spdlog
    sqlite3
)
```

---

## 注意事项

- API Key 建议通过环境变量注入，不要写死在代码中。
- 使用 Ollama 时，请确保本地服务已启动，并且模型已拉取。
- 若你只使用云端模型，可仅传入 `APIConfig`；本地模型使用 `OllamaConfig`。

## License

本项目使用仓库根目录中的 `LICENSE`。
