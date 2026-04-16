#include "../include/ChatGPTProvider.h"
#include "../include/util/myLog.h"
#include <jsoncpp/json/json.h>
#include "../include/util/httplib.h"

namespace ai_chat_sdk
{
    // ChatGPTProvider类实现
    bool ChatGPTProvider::init(const std::map<std::string, std::string> &modelConfig)
    {
        // 从modelConfig中获取必要的配置信息，例如API密钥和端点
        auto apiKeyIt = modelConfig.find("apiKey");
        auto endpointIt = modelConfig.find("endpoint");
        if (apiKeyIt == modelConfig.end() || endpointIt == modelConfig.end())
        {
            _isAvailable = false;
            if (apiKeyIt == modelConfig.end())
            {
                DEBG("ChatGPTProvider init failed: apiKey is missing in modelConfig");
            }
            if (endpointIt == modelConfig.end())
            {
                DEBG("ChatGPTProvider init failed: endpoint is missing in modelConfig");
            }
            return false;
        }
        _apiKey = apiKeyIt->second;
        _endpoint = endpointIt->second;
        // 这里可以添加更多的初始化逻辑，例如测试API连接等
        _isAvailable = true; // 假设初始化成功
        INFO("ChatGPTProvider initialized success, apiKey: {}, endpoint: {}", _apiKey, _endpoint);
        return true;
    }

    // 检测模型是否可用
    bool ChatGPTProvider::isAvailable() const
    {
        return _isAvailable;
    }

    // 获取模型名称
    std::string ChatGPTProvider::getModelName() const
    {
        return "openai/gpt-4o-mini";
    }

    // 获取模型描述信息
    std::string ChatGPTProvider::getLLMDesc() const
    {
        return "OpenAI的GPT-4o-mini是一款强大的语言模型，具有出色的自然语言理解和生成能力，适用于各种对话场景，提供流畅自然的交互体验。";
    }

    // 发送消息-全量返回
    std::string ChatGPTProvider::sendMessage(const std::vector<Message> &messages, const std::map<std::string, std::string> &requestParam)
    {
        // 1.检测模型是否可用
        if (!isAvailable())
        {
            ERRO("ChatGPTProvider sendMessage failed: model is not available");
            return "";
        }

        // 2.获取请求参数
        double temperature = 0.7;   // 默认温度参数
        int maxOutputTokens = 2048; // 默认最大输出token数
        if (requestParam.find("temperature") != requestParam.end())
        {
            temperature = std::stod(requestParam.at("temperature"));
        }
        if (requestParam.find("maxOutputTokens") != requestParam.end())
        {
            maxOutputTokens = std::stoi(requestParam.at("maxOutputTokens"));
        }

        // 3.构建消息列表
        Json::Value messagesJson(Json::arrayValue);
        for (const auto &message : messages)
        {
            Json::Value messageJson;
            messageJson["role"] = message._role;
            messageJson["content"] = message._content;
            messagesJson.append(messageJson);
        }

        // 4.构建请求参数
        Json::Value requestBody;
        requestBody["model"] = getModelName();
        requestBody["input"] = messagesJson;
        requestBody["temperature"] = temperature;
        requestBody["max_output_tokens"] = maxOutputTokens;

        // 5.序列化请求体
        Json::StreamWriterBuilder writer;
        std::string requestBodyStr = Json::writeString(writer, requestBody);

        // 6.设置HTTP请求头
        httplib::Headers headers = {
            {"Content-Type", "application/json"},
            {"Authorization", "Bearer " + _apiKey}};

        // 7.创建HTTP客户端
        httplib::Client cli(_endpoint.c_str());
        cli.set_connection_timeout(30, 0); // 设置连接超时时间为30秒
        cli.set_read_timeout(60, 0);       // 设置读取超时时间为30秒
        cli.set_proxy("127.0.0.1", 10808); // 设置代理

        // 8.发送HTTPS POST请求
        auto res = cli.Post("/api/v1/responses", headers, requestBodyStr, "application/json");
        if (!res)
        {
            ERRO("ChatGPTProvider sendMessage failed: HTTP request failed, error code: {}", res.error());
            return "";
        }

        // 9.检测响应是否成功
        if (res->status != 200)
        {
            ERRO("ChatGPTProvider sendMessage failed: HTTP response status code: {}", res->status);
            return "";
        }
        INFO("ChatGPTProvider sendMessage success: HTTP response body: {}", res->body);

        // 10.反序列化响应体
        Json::Value responseJson;
        Json::CharReaderBuilder reader;
        std::string errs;
        std::istringstream ss(res->body);
        if (!Json::parseFromStream(reader, ss, &responseJson, &errs))
        {
            ERRO("ChatGPTProvider sendMessage failed: failed to parse JSON response, error: {}", errs);
            return "";
        }

        // 11.提取模型回复内容
        if (!responseJson.isMember("output") || !responseJson["output"].isArray() || responseJson["output"].empty())
        {
            ERRO("ChatGPTProvider sendMessage failed: invalid response format, missing 'output' field");
            return "";
        }
        if (!responseJson["output"][0].isMember("content") || !responseJson["output"][0]["content"].isArray() || responseJson["output"][0]["content"].empty())
        {
            ERRO("ChatGPTProvider sendMessage failed: invalid response format, missing 'content' field in output");
            return "";
        }
        if(!responseJson["output"][0]["content"][0].isMember("text") || !responseJson["output"][0]["content"][0]["text"].isString())
        {
            ERRO("ChatGPTProvider sendMessage failed: invalid response format, missing 'text' field in content");
            return "";
        }
        std::string reply = responseJson["output"][0]["content"][0]["text"].asString();
        INFO("ChatGPTProvider sendMessage success: model reply: {}", reply);
        return reply;
    }

    // 发送消息-流式返回
    std::string ChatGPTProvider::sendMessageStream(const std::vector<Message> &messages,
                                                   const std::map<std::string, std::string> &requestParam,
                                                   std::function<void(const std::string &, bool)> callback) // callback参数：第一个参数是模型返回的增量数据，第二个参数表示是否是最后一条消息
    {

        return "";
    }

} // end of namespace ai_chat_sdk
