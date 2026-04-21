#include "../include/LLMManager.h"
#include "../include/util/myLog.h"
#include "../include/common.h"

namespace ai_chat_sdk
{
    // 注册模型提供者
    bool LLMManager::registerProvider(const std::string &modelName, std::unique_ptr<LLMProvider> provider)
    {
        // 参数检测
        if (!provider)
        {
            ERRO("Failed to register LLM provider, provider instance is null, modelName: {}", modelName);
            return false;
        }
        // unique_ptr不允许复制，只能移动
        _providers[modelName] = std::move(provider);
        _llmInfos[modelName] = LLMInfo(modelName);

        // 模型注册成功
        INFO("LLM provider registered successfully, modelName: {}", modelName);
        return true;
    }

    // 初始化指定模型
    bool LLMManager::initModel(const std::string &modelName, const std::map<std::string, std::string> &modelParams)
    {
        // 检测模型是否注册
        auto it = _providers.find(modelName);
        if (it == _providers.end())
        {
            ERRO("Failed to initialize LLM, provider not registered, modelName: {}", modelName);
            return false;
        }

        // 调用模型提供者的初始化方法
        auto isInitSuccess = it->second->init(modelParams);
        if (!isInitSuccess)
        {
            ERRO("Failed to initialize LLM, modelName: {}", modelName);
            return false;
        }
        INFO("LLM initialized successfully, modelName: {}", modelName);

        // 更新模型信息
        _llmInfos[modelName]._isAvailable = it->second->isAvailable();
        _llmInfos[modelName]._description = it->second->getLLMDesc();
        _llmInfos[modelName]._endpoint = modelParams.count("endpoint") ? modelParams.at("endpoint") : "";
        return true;
    }

    // 获取可用模型
    std::vector<LLMInfo> LLMManager::getAvailableModels() const
    {
        std::vector<LLMInfo> availableModels;
        for (const auto &pair : _llmInfos)
        {
            if (pair.second._isAvailable)
            {
                availableModels.push_back(pair.second);
            }
        }
        return availableModels;
    }

    // 检查模型是否可用
    bool LLMManager::isModelAvailable(const std::string &modelName) const
    {
        auto it = _llmInfos.find(modelName);
        if (it != _llmInfos.end())
        {
            return it->second._isAvailable;
        }
        return false;
    }

    // 发送消息-全量返回
    std::string LLMManager::sendMessage(const std::string &modelName, const std::vector<Message> &messages, const std::map<std::string, std::string> &requestParam)
    {
        // 检测模型是否注册
        auto it = _providers.find(modelName);
        if (it == _providers.end())
        {
            ERRO("Failed to send message, LLM provider not registered, modelName: {}", modelName);
            return "";
        }

        // 检测模型是否可用
        if (!isModelAvailable(modelName))
        {
            ERRO("Failed to send message, LLM not available, modelName: {}", modelName);
            return "";
        }

        // 调用模型提供者的发送消息方法
        return it->second->sendMessage(messages, requestParam);
    }

    // 发送消息-流式返回
    std::string LLMManager::sendMessageStream(const std::string &modelName, const std::vector<Message> &messages, const std::map<std::string, std::string> &requestParam, std::function<void(const std::string &, bool)> callback)
    {
        // 检测模型是否注册
        auto it = _providers.find(modelName);
        if (it == _providers.end())
        {
            ERRO("Failed to send message, LLM provider not registered, modelName: {}", modelName);
            return "";
        }

        // 检测模型是否可用
        if (!isModelAvailable(modelName))
        {
            ERRO("Failed to send message, LLM not available, modelName: {}", modelName);
            return "";
        }

        // 调用模型提供者的发送消息方法
        return it->second->sendMessageStream(messages, requestParam, callback);
    }
} // end of namespace ai_chat_sdk