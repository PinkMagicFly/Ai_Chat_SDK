# AIModelAccess / ChatSDK（C++）

一个基于 **C++17** 的多模型聊天接入项目，仓库同时包含：

- `ai_chat_SDK`：统一封装多家大模型与本地 Ollama 的 C++ SDK
- `AIChatServer`：基于 SDK 构建的 HTTP 聊天服务
- `ChatServer/build/www`：配套的前端聊天页面

如果你希望在 C++ 项目中快速接入 LLM，或者直接运行一个可访问的聊天服务，这个仓库可以作为起点。

## 仓库结构

```text
chat-sdk-access-ai-large-model/
├── AIModelAccess/
│   ├── sdk/              # Chat SDK
│   ├── ChatServer/       # HTTP 服务端 + 前端静态资源
│   └── test/             # SDK 测试与示例
├── docs/
│   └── diagrams/         # PlantUML 类图
├── README.md
└── README.en.md
```

## 架构图

下图来自 `docs/diagrams/class-architecture.puml`，展示了 SDK、会话管理、模型管理和 Provider 的关系。

![ChatSDK 类结构图](docs/diagrams/class-architecture.svg)

## 功能特性

### SDK 能力

- 统一接入多种模型
- 会话创建、查询、列举、删除
- 消息持久化
- 全量回复 `sendMessage`
- 流式回复 `sendMessageIncremental`
- SQLite 本地存储

### 当前支持的模型

- DeepSeek：`deepseek-chat`
- OpenRouter(OpenAI)：`openai/gpt-4o-mini`
- OpenRouter(Gemini)：`google/gemini-2.0-flash-001`
- Ollama 本地模型：例如 `gemma3:270m`

### ChatServer 能力

- `GET /api/sessions`：获取会话列表
- `GET /api/models`：获取可用模型
- `POST /api/session`：创建新会话
- `GET /api/session/{id}/history`：获取会话历史
- `POST /api/message`：发送消息并全量返回
- `POST /api/message/async`：发送消息并流式返回
- `DELETE /api/session/{id}`：删除会话
- 托管前端静态页面

## 环境依赖

### 编译环境

- CMake >= 3.10
- 支持 C++17 的编译器（g++ / clang++）

### 主要依赖库

- `OpenSSL`
- `jsoncpp`
- `fmt`
- `spdlog`
- `sqlite3`
- `gflags`（ChatServer）
- `gtest`（test）

Linux 环境通常需要先安装对应开发包，例如：

- `libssl-dev`
- `libjsoncpp-dev`
- `libfmt-dev`
- `libspdlog-dev`
- `libsqlite3-dev`
- `libgflags-dev`
- `libgtest-dev`

## 获取代码

```bash
git clone https://gitee.com/zhibite-edu/ai-model-acess-tech.git
# 或
git clone https://github.com/PinkMagicFly/Ai_Chat_SDK.git

cd chat-sdk-access-ai-large-model
```

如果你使用的是你自己的镜像仓库，请把地址替换成实际仓库地址。

## 一、编译 SDK

```bash
cd AIModelAccess/sdk
mkdir -p build
cd build
cmake ..
make -j
```

如果你需要安装到系统目录：

```bash
sudo make install
```

默认安装位置：

- 静态库：`/usr/local/lib`
- 头文件：`/usr/local/include/ai_chat_SDK`

## 二、编译 ChatServer

```bash
cd AIModelAccess/ChatServer
mkdir -p build
cd build
cmake ..
make -j
```

编译结果默认输出到：

- `AIModelAccess/ChatServer/build/AIChatServer`

## 三、运行前准备

### 云端模型环境变量

```bash
export deepseek_apikey="your_deepseek_key"
export openrouter_apikey="your_openrouter_key"
```

### Ollama 本地模型

如果你启用本地模型，请确保：

```bash
ollama serve
ollama pull gemma3:270m
```

## 四、启动 ChatServer

推荐在 `AIModelAccess/ChatServer/build` 目录下启动：

```bash
cd AIModelAccess/ChatServer/build
./AIChatServer
```

原因是当前服务端静态目录配置为：

```cpp
_chatServer->set_base_dir("./www");
```

因此当前工作目录需要能找到 `./www`，否则根路径无法正确映射前端页面。

启动后默认访问地址：

- `http://127.0.0.1:8807/`
- `http://127.0.0.1:8807/index.html`

## 五、ChatServer 配置方式

`AIChatServer` 支持 4 层配置优先级：

1. 命令行参数
2. `--config_file` 指定文件
3. 可执行文件目录下的 `config.conf`
4. 默认值 / 环境变量

示例 `config.conf`：

```txt
--host=0.0.0.0
--port=8807
--log_level=INFO
--temperature=0.7
--max_tokens=2048
--ollama_model_name=gemma3:270m
--ollama_model_desc=Gemma 3 local model
--ollama_endpoint=http://localhost:11434
```

帮助与版本：

```bash
./AIChatServer --help
./AIChatServer --version
```

## 六、SDK 核心接口

头文件：

```cpp
#include <ai_chat_SDK/ChatSDK.h>
```

当前 `ChatSDK` 常用接口：

- `bool initializeSDK(const std::vector<std::shared_ptr<Config>>& configs)`
- `std::string createSession(const std::string& modelName)`
- `Session getSession(const std::string& sessionId)`
- `std::vector<std::string> getSessionLists() const`
- `bool deleteSession(const std::string& sessionId)`
- `std::vector<LLMInfo> getAvailableModels() const`
- `std::string sendMessage(const std::string& sessionId, const std::string& message)`
- `std::string sendMessageIncremental(const std::string& sessionId, const std::string& message, callback)`

相关结构体见：

- `AIModelAccess/sdk/include/common.h`

包括：

- `Message`
- `Config`
- `APIConfig`
- `OllamaConfig`
- `LLMInfo`
- `Session`

## 七、SDK 快速示例

```cpp
#include <iostream>
#include <memory>
#include <vector>
#include <cstdlib>
#include <ai_chat_SDK/ChatSDK.h>

int main() {
    ai_chat_sdk::ChatSDK sdk;

    auto deepseek = std::make_shared<ai_chat_sdk::APIConfig>();
    deepseek->_modelName = "deepseek-chat";
    deepseek->_apiKey = std::getenv("deepseek_apikey");
    deepseek->_temperature = 0.7f;
    deepseek->_max_tokens = 2048;

    std::vector<std::shared_ptr<ai_chat_sdk::Config>> configs = {deepseek};

    if (!sdk.initializeSDK(configs)) {
        std::cerr << "initializeSDK failed" << std::endl;
        return 1;
    }

    auto sessionId = sdk.createSession("deepseek-chat");
    if (sessionId.empty()) {
        std::cerr << "createSession failed" << std::endl;
        return 1;
    }

    auto callback = [](const std::string& chunk, bool done) {
        std::cout << chunk;
        if (done) {
            std::cout << "\n[done]" << std::endl;
        }
    };

    auto full = sdk.sendMessageIncremental(sessionId, "你好，请介绍一下你自己。", callback);
    std::cout << "\nFull response: " << full << std::endl;
    return 0;
}
```

## 八、ChatServer API 概览

### 创建会话

```http
POST /api/session
Content-Type: application/json
```

```json
{
  "model": "deepseek-chat"
}
```

### 获取会话列表

```http
GET /api/sessions
```

### 获取模型列表

```http
GET /api/models
```

### 获取历史消息

```http
GET /api/session/{session_id}/history
```

### 删除会话

```http
DELETE /api/session/{session_id}
```

### 发送消息（全量）

```http
POST /api/message
Content-Type: application/json
```

```json
{
  "session_id": "session_xxx",
  "message": "你好"
}
```

### 发送消息（流式）

```http
POST /api/message/async
Content-Type: application/json
```

```json
{
  "session_id": "session_xxx",
  "message": "你好"
}
```

返回内容为 SSE 风格数据块，例如：

```text
data: "第一段内容"

data: "第二段内容"

data: [DONE]
```

## 九、curl 示例

### 获取模型列表

```bash
curl -s http://127.0.0.1:8807/api/models
```

### 创建会话

```bash
curl -s http://127.0.0.1:8807/api/session \
  -H 'Content-Type: application/json' \
  -d '{"model":"deepseek-chat"}'
```

### 获取会话列表

```bash
curl -s http://127.0.0.1:8807/api/sessions
```

### 发送流式消息

```bash
curl -N http://127.0.0.1:8807/api/message/async \
  -H 'Content-Type: application/json' \
  -d '{"session_id":"session_xxx","message":"请介绍一下你自己"}'
```

## 十、测试

测试与示例位于：

- `AIModelAccess/test/testLLM.cpp`

编译方式：

```bash
cd AIModelAccess/test
mkdir -p build
cd build
cmake ..
make -j
./AIModelAccessTest
```

运行前请确保：

- 环境变量已设置
- 对应模型服务可访问
- 如果使用 Ollama，本地服务已启动且模型已拉取

## 十一、使用建议

- API Key 建议通过环境变量注入，不要写死在源码中
- 生产环境建议继续补充鉴权、限流、监控和容器化部署
- 如果你只需要本地集成能力，可以直接使用 SDK，而不启动 ChatServer

## License

项目使用仓库根目录中的 [LICENSE](LICENSE)。
