#include "../include/ChatSDK.h"
#include "../include/OllamaProvider.h"
#include "../include/DeepSeekProvider.h"
#include "../include/GeminiProvider.h"
#include "../include/ChatGPTProvider.h"
#include "../include/util/myLog.h"

namespace ai_chat_sdk
{
    // 初始化SDK
    bool ChatSDK::initializeSDK(const std::vector<std::shared_ptr<Config>> &configs)
    {
        if (_initialized)
        {
            return true; // 已经初始化，直接返回
        }
        // 注册所有支持的模型提供者
        registerAllProviders(configs);
        // 初始化所有支持的模型
        initAllProviders(configs);
        _initialized = true;
        return true;
    }

    // 注册所有支持的模型
    void ChatSDK::registerAllProviders(const std::vector<std::shared_ptr<Config>> &configs)
    {
        // deepseek-chat
        if (!_llmManager.isModelAvailable("deepseek-chat"))
        {
            auto deepSeekProvider = std::make_unique<DeepSeekProvider>();
            _llmManager.registerProvider("deepseek-chat", std::move(deepSeekProvider));
            INFO("Registered DeepSeekProvider for model: deepseek-chat");
        }

        // gpt-4o-mini
        if (!_llmManager.isModelAvailable("openai/gpt-4o-mini"))
        {
            auto chatGPTProvider = std::make_unique<ChatGPTProvider>();
            _llmManager.registerProvider("openai/gpt-4o-mini", std::move(chatGPTProvider));
            INFO("Registered ChatGPTProvider for model: openai/gpt-4o-mini");
        }

        // gemini-2.0-flash-001
        if (!_llmManager.isModelAvailable("google/gemini-2.0-flash-001"))
        {
            auto geminiProvider = std::make_unique<GeminiProvider>();
            _llmManager.registerProvider("google/gemini-2.0-flash-001", std::move(geminiProvider));
            INFO("Registered GeminiProvider for model: google/gemini-2.0-flash-001");
        }

        // ollama
        for (const auto &config : configs)
        {
            // 检测config是不是ollama模型的配置，如果是，就注册ollama模型提供者
            auto ollamaconfig = std::dynamic_pointer_cast<OllamaConfig>(config); // 安全的向下转换
            if (ollamaconfig)
            { // 是ollama模型的配置，返回不是空的shared_ptr
                const std::string &modelName = ollamaconfig->_modelName;
                if (!_llmManager.isModelAvailable(modelName))
                {
                    auto ollamaProvider = std::make_unique<OllamaProvider>();
                    _llmManager.registerProvider(modelName, std::move(ollamaProvider));
                    INFO("Registered OllamaProvider for model: {}", modelName);
                }
            }
        }
    }

    // 初始化所有支持的模型
    void ChatSDK::initAllProviders(const std::vector<std::shared_ptr<Config>> &configs)
    {
        for (const auto &config : configs)
        {
            const std::string &modelName = config->_modelName;
            auto apiConfig = std::dynamic_pointer_cast<APIConfig>(config); // 安全的向下转换
            if (apiConfig)
            { // 是通过API方式接入的模型配置
                if (modelName != "deepseek-chat" && modelName != "openai/gpt-4o-mini" && modelName != "google/gemini-2.0-flash-001")
                {
                    WARN("No provider registered for API model: {}, skipping initialization", modelName);
                    continue;
                }
                initAPIModelProvider(apiConfig);
            }
            else if (auto ollamaConfig = std::dynamic_pointer_cast<OllamaConfig>(config))
            { // 是通过Ollama方式接入的模型配置
                initOllamaProvider(ollamaConfig);
            }
            else
            {
                WARN("Unknown config type for model: {}, skipping initialization", modelName);
            }
        }
    }

    // 初始化API模型提供者
    bool ChatSDK::initAPIModelProvider(const std::shared_ptr<APIConfig> &apiConfig)
    {

        // 对于API模型，必须提供modelName和apiKey，并且必须已经注册了对应的模型提供者，才能初始化成功
        if (!apiConfig || apiConfig->_apiKey.empty())
        {
            ERRO("API model config for model: {} is missing apiKey, cannot initialize", apiConfig ? apiConfig->_modelName : "unknown");
            return false;
        }
        const std::string &modelName = apiConfig->_modelName;
        if (modelName.empty())
        {
            ERRO("API model config is missing modelName, cannot initialize");
            return false;
        }

        if (_llmManager.isModelAvailable(modelName))
        {
            INFO("API model: {} is already initialized, skipping", modelName);
            return true;
        }

        // 初始化模型
        std::map<std::string, std::string> modelConfig;
        modelConfig["apiKey"] = apiConfig->_apiKey;
        if (!_llmManager.initModel(modelName, modelConfig))
        {
            ERRO("Failed to initialize API model: {}", modelName);
            return false;
        }

        // 模型配置保存
        _configMap[modelName] = apiConfig;
        INFO("Initialized API model: {} successfully", modelName);
        return true;
    }

    // 初始化Ollama模型提供者
    bool ChatSDK::initOllamaProvider(const std::shared_ptr<OllamaConfig> &ollamaConfig)
    {
        // 对于Ollama模型，必须提供modelName、endpoint和modelDesc，并且必须已经注册了对应的模型提供者，才能初始化成功
        if (!ollamaConfig || ollamaConfig->_endpoint.empty() || ollamaConfig->_modelDesc.empty())
        {
            ERRO("Ollama model config for model: {} is missing endpoint or modelDesc", ollamaConfig ? ollamaConfig->_modelName : "unknown");
            return false;
        }
        const std::string &modelName = ollamaConfig->_modelName;
        if (modelName.empty())
        {
            ERRO("Ollama model config is missing modelName, cannot initialize");
            return false;
        }
        if (_llmManager.isModelAvailable(modelName))
        {
            INFO("Ollama model: {} is already initialized, skipping", modelName);
            return true;
        }
        // 初始化模型
        std::map<std::string, std::string> modelConfig;
        modelConfig["endpoint"] = ollamaConfig->_endpoint;
        modelConfig["modelDesc"] = ollamaConfig->_modelDesc;
        modelConfig["modelName"] = modelName; // OllamaProvider的init函数需要modelName参数来获取模型描述信息
        if (!_llmManager.initModel(modelName, modelConfig))
        {
            ERRO("Failed to initialize Ollama model: {}", modelName);
            return false;
        }
        // 模型配置保存
        _configMap[modelName] = ollamaConfig;
        INFO("Initialized Ollama model: {} successfully", modelName);
        return true;
    }

    // 获取单会话的锁
    std::shared_ptr<std::mutex> ChatSDK::getSessionMutex(const std::string &sessionId)
    {
        std::lock_guard<std::mutex> lock(_sessionOpMapMutex);
        auto it = _sessionOpMap.find(sessionId);
        if (it == _sessionOpMap.end())
        {
            auto mutexPtr = std::make_shared<std::mutex>();
            _sessionOpMap[sessionId] = mutexPtr;
            return mutexPtr;
        }
        return it->second;
    }

    // 创建会话，提供模型名称
    std::string ChatSDK::createSession(const std::string &modelName)
    {
        // 创建会话之前，先检查SDK是否初始化了，模型是否可用
        if (!_initialized)
        {
            ERRO("ChatSDK createSession failed: SDK is not initialized");
            return "";
        }
        if (!_llmManager.isModelAvailable(modelName))
        {
            ERRO("ChatSDK createSession failed: model {} is not available", modelName);
            return "";
        }
        auto sessionId = _sessionManager.createSession(modelName);
        if (sessionId.empty())
        {
            ERRO("ChatSDK createSession failed: failed to create session for model {}", modelName);
            return "";
        }
        // 创建会话成功后，为该会话创建一个操作锁，保证同一时间只有一个线程在操作同一个会话
        std::lock_guard<std::mutex> lock(_sessionOpMapMutex);
        _sessionOpMap[sessionId] = std::make_shared<std::mutex>();

        INFO("ChatSDK createSession success: created session {} for model {}", sessionId, modelName);
        return sessionId;
    }

    // 获取指定会话
    Session ChatSDK::getSession(const std::string &sessionId)
    {
        if (!_initialized)
        {
            ERRO("ChatSDK getSession failed: SDK is not initialized");
            return Session("");
        }
        auto session = _sessionManager.getSession(sessionId);
        if (session._modelName.empty())
        {
            ERRO("ChatSDK getSession failed: session {} not found", sessionId);
            return Session("");
        }
        return session;
    }

    // 获取所有会话列表id
    std::vector<std::string> ChatSDK::getSessionLists() const
    {
        if (!_initialized)
        {
            ERRO("ChatSDK getSessionLists failed: SDK is not initialized");
            return {};
        }
        return _sessionManager.getSessionLists();
    }

    // 删除指定会话
    bool ChatSDK::deleteSession(const std::string &sessionId)
    {
        if (!_initialized)
        {
            ERRO("ChatSDK deleteSession failed: SDK is not initialized");
            return false;
        }
        // 删除会话之前，先获取会话对应的锁，保证同一时间只有一个线程在操作同一个会话
        auto sessionMutex = getSessionMutex(sessionId);
        std::lock_guard<std::mutex> lock(*sessionMutex);
        if (!_sessionManager.deleteSession(sessionId))
        {
            ERRO("ChatSDK deleteSession failed: failed to delete session {}", sessionId);
            return false;
        }
        // 删除会话对应的锁
        std::lock_guard<std::mutex> maplock(_sessionOpMapMutex);
        _sessionOpMap.erase(sessionId);

        INFO("ChatSDK deleteSession success: deleted session {}", sessionId);
        return true;
    }

    // 获取可用的模型信息
    std::vector<LLMInfo> ChatSDK::getAvailableModels() const
    {
        if (!_initialized)
        {
            ERRO("ChatSDK getAvailableModels failed: SDK is not initialized");
            return {};
        }
        return _llmManager.getAvailableModels();
    }

    // 发送消息-全量返回
    std::string ChatSDK::sendMessage(const std::string &sessionId, const std::string &messages)
    {
        if (!_initialized)
        {
            ERRO("ChatSDK sendMessage failed: SDK is not initialized");
            return "";
        }
        // 发送消息之前，先检查会话是否存在，获取会话使用的模型名称，然后调用模型管理器发送消息
        auto session = _sessionManager.getSession(sessionId);
        if (session._modelName.empty())
        {
            ERRO("ChatSDK sendMessage failed: session {} not found", sessionId);
            return "";
        }

        // 发送消息的过程中，可能会有多个线程同时在操作同一个会话，所以需要获取会话对应的锁，保证同一时间只有一个线程在操作同一个会话
        auto sessionMutex = getSessionMutex(sessionId);
        std::lock_guard<std::mutex> lock(*sessionMutex);

        // 先在会话中添加消息
        Message userMessage("user", messages);
        if (!_sessionManager.addMessageToSession(sessionId, userMessage))
        {
            return "";
        }
        // 构造消息列表
        std::vector<Message> messageList = _sessionManager.getSessionMessages(sessionId);

        // 构建请求参数
        auto configIt = _configMap.find(session._modelName);
        if (configIt == _configMap.end())
        {
            ERRO("ChatSDK sendMessage failed: config for model {} not found", session._modelName);
            return "";
        }
        std::map<std::string, std::string> requestParam;
        requestParam["max_tokens"] = std::to_string(configIt->second->_max_tokens);
        requestParam["temperature"] = std::to_string(configIt->second->_temperature);

        // 调用模型管理器发送消息
        auto response = _llmManager.sendMessage(session._modelName, messageList, requestParam);
        if (response.empty())
        {
            ERRO("ChatSDK sendMessage failed: model {} returned empty response", session._modelName);
            return "";
        }

        // 在会话中添加助手消息并更新时间
        Message assistantMessage("assistant", response);
        if (!_sessionManager.addMessageToSession(sessionId, assistantMessage))
        {
            return "";
        }
        if (!_sessionManager.updateSessionTimestamp(sessionId))
        {
            ERRO("ChatSDK sendMessage warning: failed to update session {} timestamp", sessionId);
        }
        INFO("ChatSDK sendMessage success: sent message to model {}, session {}", session._modelName, sessionId);
        return response;
    }

    // 发送消息-增量返回
    std::string ChatSDK::sendMessageIncremental(const std::string &sessionId, const std::string &messages, std::function<void(const std::string &, bool)> callback)
    {
        if (!_initialized)
        {
            ERRO("ChatSDK sendMessageIncremental failed: SDK is not initialized");
            return "";
        }
        // 发送消息之前，先检查会话是否存在，获取会话使用的模型名称，然后调用模型管理器发送消息
        auto session = _sessionManager.getSession(sessionId);
        if (session._modelName.empty())
        {
            ERRO("ChatSDK sendMessageIncremental failed: session {} not found", sessionId);
            return "";
        }

        // 发送消息的过程中，可能会有多个线程同时在操作同一个会话，所以需要获取会话对应的锁，保证同一时间只有一个线程在操作同一个会话
        auto sessionMutex = getSessionMutex(sessionId);
        std::lock_guard<std::mutex> lock(*sessionMutex);

        // 先在会话中添加消息
        Message userMessage("user", messages);
        if (!_sessionManager.addMessageToSession(sessionId, userMessage))
        {
            return "";
        }
        // 构造消息列表
        std::vector<Message> messageList = _sessionManager.getSessionMessages(sessionId);

        // 构建请求参数
        auto configIt = _configMap.find(session._modelName);
        if (configIt == _configMap.end())
        {
            ERRO("ChatSDK sendMessageIncremental failed: config for model {} not found", session._modelName);
            return "";
        }
        std::map<std::string, std::string> requestParam;
        requestParam["max_tokens"] = std::to_string(configIt->second->_max_tokens);
        requestParam["temperature"] = std::to_string(configIt->second->_temperature);

        // 调用模型管理器发送消息
        auto response = _llmManager.sendMessageStream(session._modelName, messageList, requestParam, callback);
        if (response.empty())
        {
            ERRO("ChatSDK sendMessageIncremental failed: model {} returned empty response", session._modelName);
            return "";
        }

        // 在会话中添加助手消息并更新时间
        Message assistantMessage("assistant", response);
        if (!_sessionManager.addMessageToSession(sessionId, assistantMessage))
        {
            return "";
        }
        if (!_sessionManager.updateSessionTimestamp(sessionId))
        {
            ERRO("ChatSDK sendMessageIncremental warning: failed to update session {} timestamp", sessionId);
        }
        INFO("ChatSDK sendMessageIncremental success: sent message to model {}, session {}", session._modelName, sessionId);
        return response;
    }

} // namespace ai_chat_sdk
