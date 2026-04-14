#include"../include/DeepSeekProvider.h"
#include"../include/util/myLog.h"
#include<jsoncpp/json/json.h>
#include"../include/util/httplib.h"

namespace ai_chat_sdk
{
    //DeepSeekProvider类实现
    bool DeepSeekProvider::init(const std::map<std::string, std::string>&modelConfig) {
        //从modelConfig中获取必要的配置信息，例如API密钥和端点
        auto apiKeyIt = modelConfig.find("apiKey");
        auto endpointIt = modelConfig.find("endpoint");
        if (apiKeyIt == modelConfig.end() || endpointIt == modelConfig.end()) {
            _isAvailable = false;
            if(apiKeyIt==modelConfig.end()){
                DEBG("DeepSeekProvider init failed: apiKey is missing in modelConfig");
            }
            if(endpointIt==modelConfig.end()){
                DEBG("DeepSeekProvider init failed: endpoint is missing in modelConfig");
            }
            return false;
        }
        _apiKey = apiKeyIt->second;
        _endpoint = endpointIt->second;
        //这里可以添加更多的初始化逻辑，例如测试API连接等
        _isAvailable = true; //假设初始化成功
        INFO("DeepSeekProvider initialized success, apiKey: {}, endpoint: {}", _apiKey, _endpoint);
        return true;    
    }

    //检测模型是否可用
    bool DeepSeekProvider::isAvailable() const {
        return _isAvailable;
    }

    //获取模型名称
    std::string DeepSeekProvider::getModelName() const {
        return "deepseek-chat";
    }

    //获取模型描述信息
    std::string DeepSeekProvider::getLLMDesc() const {
        return "一款实用性强、性能优越的中文大语言模型，适用于各种对话场景，提供流畅自然的交互体验。";
    }

    //发送消息-全量返回
    std::string DeepSeekProvider::sendMessage(const std::vector<Message>& messages, const std::map<std::string,std::string>&requestParam)
    {
        //1.检测模型是否可用
        if(!_isAvailable){
            ERRO("DeepSeekProvider sendMessage failed: model is not available");
            return "";
        }
        //2.构建请求参数
        double temperature = 0.7; //默认温度
        int maxTokens = 2048; //默认最大token数
        if(requestParam.find("temperature")!=requestParam.end()){
            try{
                temperature = std::stod(requestParam.at("temperature"));
            }catch(const std::exception& e){
                ERRO("DeepSeekProvider sendMessage failed: invalid temperature value: {}, error: {}", requestParam.at("temperature"), e.what());
                return "";
            }
        }
        if(requestParam.find("maxTokens")!=requestParam.end()){
            try{
                maxTokens = std::stoi(requestParam.at("maxTokens"));
            }catch(const std::exception& e){
                ERRO("DeepSeekProvider sendMessage failed: invalid maxTokens value: {}, error: {}", requestParam.at("maxTokens"), e.what());
                return "";
            }
        }
        //3.构造历史消息
        Json::Value MessagesArray(Json::arrayValue);
        for(const auto& msg: messages){
            Json::Value msgJson;
            msgJson["role"] = msg._role;
            msgJson["content"] = msg._content;
            MessagesArray.append(msgJson);
        }
        //4.构造请求体
        Json::Value requestBody;
        requestBody["model"] = getModelName();
        requestBody["messages"] = MessagesArray;
        requestBody["temperature"] = temperature;
        requestBody["max_tokens"] = maxTokens;
        //5.序列化
        Json::StreamWriterBuilder writer;
        std::string requestBodyStr = Json::writeString(writer, requestBody);
        INFO("DeepSeekProvider sendMessage request body: {}", requestBodyStr);
        //6.构造HTTPS客户端
        httplib::Client cli(_endpoint.c_str());
        cli.set_connection_timeout(30,0);   //设置连接超时时间为30秒
        cli.set_read_timeout(30,0);         //设置读取响应超时时间为30秒
        //7.构造请求头
        httplib::Headers headers = {
            {"Content-Type", "application/json"},
            {"Authorization", "Bearer " + _apiKey}
        };
        //8.发送POST请求
        auto res = cli.Post("/v1/chat/completions", headers, requestBodyStr, "application/json");
        //9.处理响应
        if (!res) {
            ERRO("DeepSeekProvider sendMessage failed: no response from server, error code: {}", res.error());
            return "";
        }
        INFO("DeepSeekProvider sendMessage response status: {}\nbody: {}", res->status, res->body);
        //10.检测响应是否成功
        if (res->status != 200) {
            ERRO("DeepSeekProvider sendMessage failed: server returned non-200 status code");
            return "";
        }
        //11.解析（反序列化）响应体
        Json::CharReaderBuilder reader;
        Json::Value responseBody;
        std::string errs;
        std::istringstream ss(res->body);
        if (!Json::parseFromStream(reader, ss, &responseBody, &errs)) {
            ERRO("DeepSeekProvider sendMessage failed: failed to parse response body, error: {}", errs);
            return "";
        }
        //12.获取响应中的choices字段
        const Json::Value& messagesArray = responseBody["choices"];
        if (!messagesArray.isArray() || messagesArray.empty()|| !messagesArray[0].isMember("message")) {
            ERRO("DeepSeekProvider sendMessage failed: response body does not contain valid choices array");
            return "";
        }
        //13.获取choice中的message字段
        const Json::Value& message = messagesArray[0]["message"];
        if (!message.isObject() || !message.isMember("content")) {
            ERRO("DeepSeekProvider sendMessage failed: choice does not contain valid message object with content field");
            return "";
        }
        //14.返回message中的content字段
        std::string content = message["content"].asString();
        INFO("DeepSeekProvider sendMessage success, response content: {}", content);
        return content;
    }
    //发送消息-流式返回
    std::string DeepSeekProvider::sendMessageStream(const std::vector<Message>& messages, 
                                   const std::map<std::string,std::string>&requestParam,
                                   std::function<void(const std::string&,bool)>callbakc)//callback参数：第一个参数是模型返回的增量数据，第二个参数表示是否是最后一条消息
    {
        return ""; //流式返回功能暂未实现
    }

}