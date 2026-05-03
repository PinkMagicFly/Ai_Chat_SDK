# AIModelAccess / ChatSDK (C++)

This repository is a **C++17** multi-model chat access project that includes:

- `ai_chat_SDK`: a unified C++ SDK for cloud LLMs and local Ollama models
- `AIChatServer`: an HTTP chat service built on top of the SDK
- `ChatServer/build/www`: a browser-based frontend page served by the backend

It can be used either as a C++ SDK integration baseline or as a runnable chat service with HTTP APIs and a built-in frontend.

## Repository Layout

```text
chat-sdk-access-ai-large-model/
├── AIModelAccess/
│   ├── sdk/
│   ├── ChatServer/
│   └── test/
├── docs/
│   └── diagrams/
├── README.md
└── README.en.md
```

## Architecture Diagram

The following diagram is generated from `docs/diagrams/class-architecture.puml` and shows the relationship between `ChatSDK`, session management, model management, and providers.

![ChatSDK class architecture](docs/diagrams/class-architecture.svg)

## Features

### SDK capabilities

- Unified access to multiple LLM providers
- Session creation, lookup, listing, and deletion
- Message persistence
- Full-response mode via `sendMessage`
- Streaming-response mode via `sendMessageIncremental`
- SQLite-based local storage

### Currently supported models

- DeepSeek: `deepseek-chat`
- OpenRouter (OpenAI): `openai/gpt-4o-mini`
- OpenRouter (Gemini): `google/gemini-2.0-flash-001`
- Local Ollama models, such as `gemma3:270m`

### ChatServer capabilities

- `GET /api/sessions`
- `GET /api/models`
- `POST /api/session`
- `GET /api/session/{id}/history`
- `POST /api/message`
- `POST /api/message/async`
- `DELETE /api/session/{id}`
- Static frontend hosting

## Requirements

### Build environment

- CMake >= 3.10
- A C++17 compiler

### Main dependencies

- `OpenSSL`
- `jsoncpp`
- `fmt`
- `spdlog`
- `sqlite3`
- `gflags` for `ChatServer`
- `gtest` for `test`

## Clone

```bash
git clone https://gitee.com/zhibite-edu/ai-model-acess-tech.git
# or
git clone https://github.com/PinkMagicFly/Ai_Chat_SDK.git

cd chat-sdk-access-ai-large-model
```

If you are using your own mirror or fork, replace the URL accordingly.

## 1. Build the SDK

```bash
cd AIModelAccess/sdk
mkdir -p build
cd build
cmake ..
make -j
```

To install:

```bash
sudo make install
```

Default install paths:

- Static library: `/usr/local/lib`
- Headers: `/usr/local/include/ai_chat_SDK`

## 2. Build ChatServer

```bash
cd AIModelAccess/ChatServer
mkdir -p build
cd build
cmake ..
make -j
```

Default output:

- `AIModelAccess/ChatServer/build/AIChatServer`

## 3. Runtime Preparation

### Cloud model environment variables

```bash
export deepseek_apikey="your_deepseek_key"
export openrouter_apikey="your_openrouter_key"
```

### Ollama local model

If you use local models:

```bash
ollama serve
ollama pull gemma3:270m
```

## 4. Start ChatServer

It is recommended to start the server from `AIModelAccess/ChatServer/build`:

```bash
cd AIModelAccess/ChatServer/build
./AIChatServer
```

The current static directory is configured as:

```cpp
_chatServer->set_base_dir("./www");
```

So the current working directory must contain `./www`, otherwise the frontend root path will not be served correctly.

Default access URLs:

- `http://127.0.0.1:8807/`
- `http://127.0.0.1:8807/index.html`

## 5. ChatServer Configuration

`AIChatServer` supports four levels of configuration priority:

1. Command line flags
2. `--config_file`
3. `config.conf` in the executable directory
4. Defaults / environment variables

Example `config.conf`:

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

Help and version:

```bash
./AIChatServer --help
./AIChatServer --version
```

## 6. SDK Core API

Header:

```cpp
#include <ai_chat_SDK/ChatSDK.h>
```

Current `ChatSDK` methods:

- `bool initializeSDK(const std::vector<std::shared_ptr<Config>>& configs)`
- `std::string createSession(const std::string& modelName)`
- `Session getSession(const std::string& sessionId)`
- `std::vector<std::string> getSessionLists() const`
- `bool deleteSession(const std::string& sessionId)`
- `std::vector<LLMInfo> getAvailableModels() const`
- `std::string sendMessage(const std::string& sessionId, const std::string& message)`
- `std::string sendMessageIncremental(const std::string& sessionId, const std::string& message, callback)`

Related data structures are defined in:

- `AIModelAccess/sdk/include/common.h`

Including:

- `Message`
- `Config`
- `APIConfig`
- `OllamaConfig`
- `LLMInfo`
- `Session`

## 7. SDK Quick Example

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

    auto full = sdk.sendMessageIncremental(sessionId, "Hello, please introduce yourself.", callback);
    std::cout << "\nFull response: " << full << std::endl;
    return 0;
}
```

## 8. ChatServer API Overview

### Create session

```http
POST /api/session
Content-Type: application/json
```

```json
{
  "model": "deepseek-chat"
}
```

### List sessions

```http
GET /api/sessions
```

### List models

```http
GET /api/models
```

### Get session history

```http
GET /api/session/{session_id}/history
```

### Delete session

```http
DELETE /api/session/{session_id}
```

### Send message (full response)

```http
POST /api/message
Content-Type: application/json
```

```json
{
  "session_id": "session_xxx",
  "message": "Hello"
}
```

### Send message (streaming)

```http
POST /api/message/async
Content-Type: application/json
```

```json
{
  "session_id": "session_xxx",
  "message": "Hello"
}
```

The streaming response is sent in SSE-style chunks, for example:

```text
data: "first chunk"

data: "second chunk"

data: [DONE]
```

## 9. curl Examples

### Get model list

```bash
curl -s http://127.0.0.1:8807/api/models
```

### Create session

```bash
curl -s http://127.0.0.1:8807/api/session \
  -H 'Content-Type: application/json' \
  -d '{"model":"deepseek-chat"}'
```

### Get session list

```bash
curl -s http://127.0.0.1:8807/api/sessions
```

### Send streaming message

```bash
curl -N http://127.0.0.1:8807/api/message/async \
  -H 'Content-Type: application/json' \
  -d '{"session_id":"session_xxx","message":"Please introduce yourself"}'
```

## 10. Tests

Integration-style tests and usage examples are located in:

- `AIModelAccess/test/testLLM.cpp`

Build them with:

```bash
cd AIModelAccess/test
mkdir -p build
cd build
cmake ..
make -j
./AIModelAccessTest
```

Before running tests, make sure:

- required environment variables are set
- target model services are reachable
- Ollama is running if local models are used

## 11. Notes

- Prefer environment variables for API keys; do not hardcode secrets
- For production use, consider adding auth, rate limiting, monitoring, and deployment automation
- If you only need embedding into your own application, you can use the SDK without starting `ChatServer`

## License

See [LICENSE](LICENSE) in the repository root.
