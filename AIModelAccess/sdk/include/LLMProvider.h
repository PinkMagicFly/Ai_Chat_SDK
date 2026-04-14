#pragma once
#include<string>
#include<map>
#include"common.h"
#include<vector>
#include<functional>


namespace ai_chat_sdk{
    //LLMProvider类
class LLMProvider {
public:
    //初始化模型
    virtual bool init(const std::map<std::string, std::string>&modelConfig) =0;
    //获取模型描述信息  
    virtual std::string getLLMDesc() const = 0;
    //获取模型名字
    virtual std::string getModelName() const = 0;
    //模型是否有效
    virtual bool isAvailable() const = 0;
    //发送消息-全量返回
    virtual std::string sendMessage(const std::vector<Message>& messages, const std::map<std::string,std::string>&requestParam) = 0;
    //发送消息-流式返回
    virtual std::string sendMessageStream(const std::vector<Message>& messages, 
                                   const std::map<std::string,std::string>&requestParam,
                                   std::function<void(const std::string&,bool)>callbakc)=0;//callback参数：第一个参数是模型返回的增量数据，第二个参数表示是否是最后一条消息
protected:
    bool _isAvailable=false;//模型是否有效
    std::string _apiKey; //API密钥
    std::string _endpoint; //模型API端点 base url
};
}