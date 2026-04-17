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
        //1.检测模型是否可用
        if (!isAvailable())
        {
            ERRO("ChatGPTProvider sendMessageStream failed: model is not available");
        }

        //2.获取请求参数
        double temperature = 0.7;   // 默认温度参数
        int max_output_tokens = 2048; // 默认最大输出token数
        if (requestParam.find("temperature") != requestParam.end())
        {
            temperature = std::stod(requestParam.at("temperature"));
        }
        if (requestParam.find("max_out_tokens") != requestParam.end())
        {
            max_output_tokens = std::stoi(requestParam.at("max_out_tokens"));
        }

        //3.构建消息列表
        Json::Value messagesJson(Json::arrayValue);
        for (const auto &message : messages)
        {
            Json::Value messageJson;
            messageJson["role"] = message._role;
            messageJson["content"] = message._content;
            messagesJson.append(messageJson);
        }

        //4.构建请求参数
        Json::Value requestBody;
        requestBody["model"] = getModelName();
        requestBody["input"] = messagesJson;
        requestBody["temperature"] = temperature;
        requestBody["max_output_tokens"] = max_output_tokens;
        requestBody["stream"] = true; // 设置流式返回

        //5.序列化请求体
        Json::StreamWriterBuilder writer;
        std::string requestBodyStr = Json::writeString(writer, requestBody);

        //6.设置HTTP请求头
        httplib::Headers headers = {
            {"Content-Type", "application/json"},
            {"Authorization", "Bearer " + _apiKey},
            {"Accept", "text/event-stream"}}; // 设置接受流式响应
        
        //7.创建HTTP客户端
        httplib::Client cli(_endpoint.c_str());
        cli.set_connection_timeout(60, 0); // 设置连接超时时间为60秒
        cli.set_read_timeout(300, 0);        // 设置读取超时时间为300秒
        cli.set_proxy("127.0.0.1", 10808); // 设置代理

        //8.流式处理相关变量
        std::string buffer; // 用于缓存接收到的数据
        bool gotError = false; // 是否发生错误
        std::string errorMsg; // 错误信息
        int statusCode = 0; // HTTP状态码
        bool streamFinished = false; // 流式返回是否结束
        std::string fullReply; // 用于拼接完整回复

        //9.创建请求对象
        httplib::Request req;
        req.method = "POST";
        req.path = "/api/v1/responses";
        req.headers = headers;
        req.body = requestBodyStr;

        //10.设置响应处理器
        req.response_handler = [&](const httplib::Response &res) {
            statusCode = res.status;
            if (statusCode != 200)
            {
                gotError = true;
                errorMsg = "HTTP response status code: " + std::to_string(statusCode);
                ERRO("ChatGPTProvider sendMessageStream failed: {}", errorMsg);
                return false; // 停止接收数据
            }
            return true; // 继续接收数据
        };

        //11.设置内容接收器
        req.content_receiver = [&](const char *data, size_t data_length,size_t offset, size_t total_length) {
            //如果http请求已经发生错误，则不再处理接收到的数据
            if (gotError)
            {
                return false; // 停止接收数据
            }
            //将接收到的数据追加到buffer中
            buffer.append(data, data_length);
            DEBG("ChatGPTProvider sendMessageStream buffer: {}", buffer);
            //检查是否收到了完整的一条消息（以\n\n结尾）
            size_t pos=0;
            while((pos=buffer.find("\n\n"))!=std::string::npos)
            {
                std::string chunk=buffer.substr(0,pos);
                buffer.erase(0,pos+2); //从buffer中删除已经处理的消息

                //解析该块响应数据中模型返回的有效数据
                //处理空行和注释，注意，以:开头的行是注释行，需要忽略
                if (chunk.empty() || chunk[0] == ':') {
                    continue; //跳过空行和注释行
                }

                //获取模型返回的增量数据，增量数据以"data: "开头
                const std::string dataPrefix="data: ";
                if (chunk.compare(0, dataPrefix.size(), dataPrefix) == 0) {
                    std::string data=chunk.substr(dataPrefix.size()); //提取增量数据

                    //检测增量数据是否是流式响应的结束标志，通常是一个特殊的字符串，例如"[DONE]"
                    if (data == "[DONE]") {
                        streamFinished = true;
                        return true;//流式响应结束了
                    }

                    //反序列化增量数据，提取其中的content字段
                    Json::CharReaderBuilder reader;
                    Json::Value dataJson;
                    std::string errs;
                    std::istringstream ss(data);
                    if (!Json::parseFromStream(reader, ss, &dataJson, &errs)) {
                        WARN("DeepSeekProvider sendMessageStream failed: failed to parse incremental data, error: {}", errs);
                        continue; //解析增量数据失败了，跳过这个数据块，继续处理后续的数据块
                    }

                    //检查事件类型
                    if (!dataJson.isMember("type") || !dataJson["type"].isString()) {
                        WARN("ChatGPTProvider sendMessageStream failed: incremental data does not contain valid 'event' field");
                        continue; //增量数据中没有事件类型，跳过这个数据块
                    }
                    std::string eventType = dataJson["type"].asString();
                    if(eventType!="response.output_text.delta")
                    {
                        continue; //只处理文本增量数据，其他类型的增量数据（例如元数据增量）暂时不处理
                    }

                    //确认delta字段存在且有效
                    if (!dataJson.isMember("delta") ||  dataJson["delta"].empty() ) {
                        WARN("ChatGPTProvider sendMessageStream failed: incremental data does not contain valid 'delta' field");
                        continue; //跳过这个数据块，继续处理后续的数据块
                    }
                    
                    std::string incrementalContent=dataJson["delta"].asString();
                    fullReply+=incrementalContent;//将增量内容追加到完整回复中
                    //调用回调函数，将增量内容传递给用户
                    callback(incrementalContent,false);//false表示这不是最后一条消息
                    
                }
            }
            return true; // 继续接收数据
        };

        //12.发送HTTP请求
        auto res = cli.send(req);
        if (!res)        {
            ERRO("ChatGPTProvider sendMessageStream failed: HTTP request failed, error code: {}", res.error());
            return "";
        }

        //13.确保流式返回结束
        if(!streamFinished)
        {
            WARN("ChatGPTProvider sendMessageStream failed: stream ended without receiving end signal");
            callback("", true); //触发回调，通知流式返回结束
            return fullReply; //虽然流式返回没有正常结束了，但还是返回已经接收到的内容，避免用户完全收不到回复了
        }
        return fullReply;
    }

} // end of namespace ai_chat_sdk
