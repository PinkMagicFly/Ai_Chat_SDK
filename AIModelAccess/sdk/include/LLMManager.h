#pragma once

#include<map>
#include<memory>
#include"LLMProvider.h"

namespace ai_chat_sdk
{
    class LLMManager
    {
    public:
        //注册模型提供者
        bool registerProvider(const std::string &modelName, std::unique_ptr<LLMProvider> provider);
        //初始化指定模型
        bool initModel(const std::string &modelName, const std::map<std::string, std::string> &modelParams);
        //获取可用模型
        std::vector<LLMInfo> getAvailableModels() const;
        //检查模型是否可用
        bool isModelAvailable(const std::string &modelName) const;
        //发送消息-全量返回
        std::string sendMessage(const std::string&modelName,const std::vector<Message>&messages,const std::map<std::string,std::string>&requestParam);
        //发送消息-流式返回
        std::string sendMessageStream(const std::string&modelName,const std::vector<Message>&messages,const std::map<std::string,std::string>&requestParam,std::function<void(const std::string&,bool)>callback);//callback参数：第一个参数是模型返回的增量数据，第二个参数表示是否是最后一条消息
    private:
        //key：模型名称，value：模型提供者实例
        std::map<std::string, std::unique_ptr<LLMProvider>> _providers;
        //key:模型名称，value:模型信息
        std::map<std::string,LLMInfo> _llmInfos; 
    };
} // end of namespace ai_chat_sdk