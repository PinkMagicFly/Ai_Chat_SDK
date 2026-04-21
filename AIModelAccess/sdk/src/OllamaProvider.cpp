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
        if (requestParam.find("maxTokens") != requestParam.end())
        {
            num_ctx = std::stoi(requestParam.at("maxTokens"));
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
        //1.检测模型是否可用
        if (!isAvailable())
        {
            ERRO("OllamaProvider sendMessageStream failed: model is not available");
            return "";
        }

        //2.构建请求参数
        double temperature = 0.7; //默认温度
        int num_ctx = 2048;       //默认最大token数
        if (requestParam.find("temperature") != requestParam.end())        {
            temperature = std::stod(requestParam.at("temperature"));
        }
        if (requestParam.find("maxTokens") != requestParam.end())        {
            num_ctx = std::stoi(requestParam.at("maxTokens"));
        }

        //3.构造历史消息
        Json::Value messagesArray(Json::arrayValue);
        for (const auto &msg : messages)        {
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
        requestBody["stream"] = true; // 流式返回

        //5.序列化请求体
        Json::StreamWriterBuilder writer;
        writer["indentation"] = ""; // 去掉默认的缩进和换行，使输出的JSON字符串更紧凑
        std::string requestBodyStr = Json::writeString(writer, requestBody);

        //6.构造请求头
        httplib::Headers headers;
        headers.insert({"Content-Type", "application/json"});
        headers.insert({"Accept", "text/event-stream"});

        //7.构造HTTP客户端
        httplib::Client cli(_endpoint.c_str());
        cli.set_connection_timeout(30, 0); // 设置连接超时时间为30
        cli.set_read_timeout(300, 0);      // 流式返回时,可能需要更长的读取超时时间，这里设置为300秒

        //8.流式处理变量
        std::string buffer;             // 用于缓存接收到的数据
        std::string responseContent;    // 用于保存最终的响应内容
        bool streamFinished = false;    // 流式响应是否已经结束的标志
        bool gotError = false;          // 标记响应是否成功
        std::string errorMsg;           //错误信息
        int statusCode = 0;               //HTTP状态码

        //9.创建请求对象
        httplib::Request req;
        req.method = "POST";
        req.path = "/api/chat";
        req.headers = headers;
        req.body = requestBodyStr;

        //10.设置响应处理器
        req.response_handler = [&](const httplib::Response &res) {
            statusCode = res.status;
            if (res.status != 200)
            {
                gotError = true;
                errorMsg = "server returned non-200 status code: " + std::to_string(statusCode);
                ERRO("OllamaProvider sendMessageStream failed: {}", errorMsg);
                return false; //停止接收数据了
            }
            return true; //继续接收数据
        };

        //11.设置流式数据处理器
        req.content_receiver = [&](const char *data, size_t data_length,size_t offset, size_t total_length) {
             if(gotError){
                return false; //停止接收数据了
            }
            buffer.append(data, data_length); //将接收到的数据追加到buffer中
            INFO("OllamaProvider sendMessageStream buffer: {}", buffer);

            //解析buffer中的数据块，数据块之间以"\n"分隔
            size_t pos;
            while ((pos = buffer.find("\n")) != std::string::npos)
            {
                std::string chunk = buffer.substr(0, pos); //获取一个完整的数据块
                buffer.erase(0, pos + 1);                   //移除已处理的数据块和分隔符

                //解析该块响应数据中模型返回的有效数据
                //处理空行和注释，注意，以:开头的行是注释行，需要忽略
                if (chunk.empty() || chunk[0] == ':') {
                    continue; //跳过空行和注释行
                }

                //获取模型返回的增量数据，增量数据以"data: "开头
                const std::string dataPrefix = "";//Ollama的流式响应数据块中直接就是增量数据，没有特定的前缀了，如果有特定前缀的话，可以在这里进行判断和提取
                if (chunk.compare(0, dataPrefix.size(), dataPrefix) == 0) {
                    std::string data = chunk.substr(dataPrefix.size()); //提取增量数据

                    //反序列化增量数据，提取其中的字段
                    Json::CharReaderBuilder reader;
                    Json::Value dataJson;
                    std::string errs;
                    std::istringstream ss(data);
                    if (!Json::parseFromStream(reader, ss, &dataJson, &errs)) {
                        WARN("OllamaProvider sendMessageStream failed: failed to parse incremental data, error: {}", errs);
                        continue; //解析增量数据失败了，跳过这个数据块，继续处理后续的数据块
                    }

                    //检测done字段是否存在且为true，如果done字段存在且为true，表示流式响应结束了
                    if (dataJson.isMember("done") && dataJson["done"].isBool() && dataJson["done"].asBool()) {
                        streamFinished = true;
                        callback("", true); //通知调用者流式响应结束了
                        return true;//流式响应结束了，停止接收数据了
                    }

                    //确认content字段存在且有效
                    if (!dataJson.isMember("message") || !dataJson["message"].isObject() || !dataJson["message"].isMember("content") || !dataJson["message"]["content"].isString()) {
                        WARN("OllamaProvider sendMessageStream failed: incremental data does not contain valid message object with content field");
                        continue; //增量数据格式不正确，跳过这个数据块，继续处理后续的数据块
                    }

                    std::string incrementalContent = dataJson["message"]["content"].asString();
                    responseContent += incrementalContent; //将增量内容追加到最终响应内容中
                    callback(incrementalContent, false); //将增量内容通过回调函数传递给调用者，第二个参数表示这不是最后一条消息
                }
            }
            return true;    
        };

        //12.发送请求
        auto result = cli.send(req);
        if (!result)        {
            ERRO("OllamaProvider sendMessageStream failed: request failed, error code: {}", result.error());
            return "";
        }

        //13.确保流式响应已经正常结束了，才返回最终的响应内容
        if(!streamFinished){
            WARN("OllamaProvider sendMessageStream failed: stream finished flag is not set, but response has ended");
            callback("", true);//通知调用者流式响应结束了
            return "";
        }
        
        return responseContent; //返回最终的响应内容
    }
}