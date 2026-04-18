#include "../include/OllamaProvider.h"
#include "../include/util/myLog.h"
#include <jsoncpp/json/json.h>
#include "../include/util/httplib.h"

namespace ai_chat_sdk
{
    // 初始化模型
    bool OllamaProvider::init(const std::map<std::string, std::string> &modelConfig)
    {
        // 初始化模型名称
        if (modelConfig.find("modelName") == modelConfig.end())
        {
            ERRO("OllamaProvider init failed: modelName is required in modelConfig");
            return false;
        }
        _modelName = modelConfig.at("modelName");
        // 初始化模型描述信息
        if (modelConfig.find("modelDesc") == modelConfig.end())
        {
            ERRO("OllamaProvider init failed: modelDesc is required in modelConfig");
            return false;
        }
        _modelDesc = modelConfig.at("modelDesc");
        // 初始化_endpoint
        if (modelConfig.find("endpoint") == modelConfig.end())
        {
            ERRO("OllamaProvider init failed: endpoint is required in modelConfig");
            return false;
        }
        _endpoint = modelConfig.at("endpoint");

        _isAvailable = true; // 假设初始化成功
        INFO("OllamaProvider initialized success, modelName: {}, modelDesc: {}, endpoint: {}", _modelName, _modelDesc, _endpoint);
        return true;
    }
    // 获取模型描述信息
    std::string OllamaProvider::getLLMDesc() const
    {
        return _modelDesc;
    }
    // 获取模型名字
    std::string OllamaProvider::getModelName() const
    {
        return _modelName;
    }
    // 模型是否有效
    bool OllamaProvider::isAvailable() const
    {
        return _isAvailable;
    }
    // 发送消息-全量返回
    std::string OllamaProvider::sendMessage(const std::vector<Message> &messages, const std::map<std::string, std::string> &requestParam)
    {
        // 1.检测模型是否可用
        if (!isAvailable())
        {
            ERRO("OllamaProvider sendMessage failed: model is not available");
            return "";
        }

        // 2.构建请求参数
        double temperature = 0.7; // 默认温度
        int num_ctx = 2048;       // 默认最大token数
        if (requestParam.find("temperature") != requestParam.end())
        {
            temperature = std::stod(requestParam.at("temperature"));
        }
        if (requestParam.find("num_ctx") != requestParam.end())
        {
            num_ctx = std::stoi(requestParam.at("num_ctx"));
        }
        // 3.构造历史消息
        Json::Value messagesArray(Json::arrayValue);
        for (const auto &msg : messages)
        {
            Json::Value msgJson;
            msgJson["role"] = msg._role;
            msgJson["content"] = msg._content;
            messagesArray.append(msgJson);
        }

        // 4.构造请求体
        Json::Value options;
        options["temperature"] = temperature;
        options["num_ctx"] = num_ctx;
        Json::Value requestBody;
        requestBody["model"] = getModelName();
        requestBody["messages"] = messagesArray;
        requestBody["options"] = options;
        requestBody["stream"] = false; // 全量返回

        // 5.序列化请求体
        Json::StreamWriterBuilder writer;
        writer["indentation"] = ""; // 去掉默认的缩进和换行，使输出的JSON字符串更紧凑
        std::string requestBodyStr = Json::writeString(writer, requestBody);

        // 6.构造请求头
        httplib::Headers headers;
        headers.insert({"Content-Type", "application/json"});

        // 7.构造HTTP客户端
        httplib::Client cli(_endpoint.c_str());
        cli.set_connection_timeout(30, 0); // 设置连接超时时间为30秒
        cli.set_read_timeout(60, 0);       // 设置读取响应超时时间为60秒

        // 8.发送请求
        auto res = cli.Post("/api/chat", headers, requestBodyStr, "application/json");
        if (!res)
        {
            ERRO("OllamaProvider sendMessage failed: request failed, error code: {}", res.error());
            return "";
        }
        if (res->status != 200)
        {
            ERRO("OllamaProvider sendMessage failed: server returned non-200 status code: {}, response body: {}", res->status, res->body);
            return "";
        }
        INFO("OllamaProvider sendMessage got response, status code: {}, response body: {}", res->status, res->body);

        // 9.解析响应
        Json::CharReaderBuilder reader;
        Json::Value resJson;
        std::string errs;
        std::istringstream ss(res->body);
        if (!Json::parseFromStream(reader, ss, &resJson, &errs))
        {
            ERRO("OllamaProvider sendMessage failed: failed to parse response JSON, error: {}", errs);
            return "";
        }
        //获取响应中的message字段，提取message中的content字段
        if (!resJson.isMember("message") || !resJson["message"].isObject() || !resJson["message"].isMember("content") || !resJson["message"]["content"].isString())
        {
            ERRO("OllamaProvider sendMessage failed: response JSON does not contain valid message object with content field");
            return "";
        }
        std::string content = resJson["message"]["content"].asString();

        // 10.返回content字段的值作为最终的响应内容
        return content;

    }
    // 发送消息-流式返回
    std::string OllamaProvider::sendMessageStream(const std::vector<Message> &messages,
                                                  const std::map<std::string, std::string> &requestParam,
                                                  std::function<void(const std::string &, bool)> callback)
    {
        return "";
    }
}