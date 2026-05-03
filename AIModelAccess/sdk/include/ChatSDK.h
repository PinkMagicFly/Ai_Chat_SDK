#pragma once
#include <string>
#include <map>
#include <vector>
#include <functional>
#include "LLMManager.h"
#include "SessionManager.h"
#include "DataManager.h"
#include "common.h"

namespace ai_chat_sdk
{
    // ChatSDK类
    class ChatSDK
    {
    public:
        // 初始化SDK
        bool initializeSDK(const std::vector<std::shared_ptr<Config>> &configs);
        // 创建会话，提供模型名称
        std::string createSession(const std::string &modelName);
        // 获取指定会话
        Session getSession(const std::string &sessionId);
        // 获取所有会话列表id
        std::vector<std::string> getSessionLists() const;
        // 删除指定会话
        bool deleteSession(const std::string &sessionId);
        // 获取可用的模型信息
        std::vector<LLMInfo> getAvailableModels() const;
        // 发送消息-全量返回
        std::string sendMessage(const std::string &sessionId, const std::string &messages);
        // 发送消息-增量返回
        std::string sendMessageIncremental(const std::string &sessionId, const std::string &messages, std::function<void(const std::string &, bool)> callback); // callback参数：第一个参数是模型返回的增量数据，第二个参数表示是否是最后一条消息
        
    private:
        // 注册所支持的模型提供者
        void registerAllProviders(const std::vector<std::shared_ptr<Config>> &configs);
        // 初始化所支持的模型
        void initAllProviders(const std::vector<std::shared_ptr<Config>> &configs);
        //初始化API模型提供者
        bool initAPIModelProvider(const std::shared_ptr<APIConfig>& apiConfig);
        //初始化Ollama模型提供者
        bool initOllamaProvider(const std::shared_ptr<OllamaConfig>& ollamaConfig);
        std::shared_ptr<std::mutex> getSessionMutex(const std::string& sessionId);

    private:
        LLMManager _llmManager;                                              // 模型交互
        std::mutex _sessionOpMapMutex;                                       // 保护会话操作的线程安全
        std::unordered_map<std::string,std::shared_ptr<std::mutex>> _sessionOpMap; // 会话操作锁,key:会话ID，value:对应会话的操作锁，保证同一时间只有一个线程在操作同一个会话
        bool _initialized = false;                                           // SDK是否初始化
        std::unordered_map<std::string, std::shared_ptr<Config>> _configMap; // 模型配置,key:模型名称，value:模型配置
        SessionManager _sessionManager;                                      // 会话管理
    };
} // namespace ai_chat_sdk