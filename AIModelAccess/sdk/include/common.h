#pragma once

#include <string>
#include <ctime>
#include<vector>

namespace ai_chat_sdk {
struct Message {
  std::string _messageid; //消息id
  std::string _role;      // "user", "assistant", "system"
  std::string _content;   //消息内容
  std::time_t _timestamp; //消息发送时间戳

  Message(const std::string &role, const std::string &content)
      : _role(role), _content(content) {}
};
//模型的公共配置信息
struct Config {
  std::string _modelName; //模型名称
  int _max_tokens=2048;    //生成文本的最大长度
  float _temperature=0.7; //生成文本的随机程度，值越大越随机，通常在0.0到1.0之间
};

//通过API方式接入云端模型
struct APIConfig : public Config 
{
  std::string _apiKey; //API密钥
};

//通过Ollama接入本地模型 不需要API_KEY

//LLM信息
struct LLMInfo {
  std::string _modelName;   //模型名称
  std::string _version;     //模型版本
  std::string _description; //模型描述信息
  std::string _provider;    //模型提供商
  std::string _endpoint;    //模型API端点 base url
  bool _isAvailable=false;  //模型是否可用
  
  LLMInfo(const std::string &modelName="", const std::string &version="",
          const std::string &description="", const std::string &provider="",
          const std::string &endpoint="")
      : _modelName(modelName), _version(version), _description(description),
        _provider(provider), _endpoint(endpoint) {}
};

//会话信息
struct Session{
  std::string _sessionId; //会话ID
  std::string _modelName;  //使用的模型名称
  std::vector<Message> _messages; //会话中的消息列表
  std::time_t _createdAt; //会话创建时间
  std::time_t _updatedAt; //会话更新时间

  Session(const std::string &modelName)
      :_modelName(modelName) 
      {}
};

} // namespace ai_chat_sdk 