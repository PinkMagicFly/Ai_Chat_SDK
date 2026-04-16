#include"../include/DeepSeekProvider.h"
#include"../include/util/myLog.h"
#include<jsoncpp/json/json.h>
#include"../include/util/httplib.h"

namespace ai_chat_sdk
{
    //DeepSeekProvider类实现
    //初始化模型
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
            }
        }
        if(requestParam.find("maxTokens")!=requestParam.end()){
            try{
                maxTokens = std::stoi(requestParam.at("maxTokens"));
            }catch(const std::exception& e){
                ERRO("DeepSeekProvider sendMessage failed: invalid maxTokens value: {}, error: {}", requestParam.at("maxTokens"), e.what());
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
        writer["indentation"] = ""; //去除默认的缩进和换行，使输出更紧凑
        std::string requestBodyStr = Json::writeString(writer, requestBody);
        INFO("DeepSeekProvider sendMessage request body: {}", requestBodyStr);
        //6.构造HTTPS客户端
        httplib::Client cli(_endpoint.c_str());
        cli.set_connection_timeout(30,0);   //设置连接超时时间为30秒
        cli.set_read_timeout(30,0);         //设置读取响应超时时间为30秒
        //7.构造请求头
        httplib::Headers headers = {
            {"Content-Type", "application/json"},
            {"Authorization", "Bearer " + _apiKey},
            {"Accept", "application/json"}
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
        //INFO("DeepSeekProvider sendMessage success, response content: {}", content);
        return content;
    }
    //发送消息-流式返回
    std::string DeepSeekProvider::sendMessageStream(const std::vector<Message>& messages, 
                                   const std::map<std::string,std::string>&requestParam,
                                   std::function<void(const std::string&,bool)>callback)//callback参数：第一个参数是模型返回的增量数据，第二个参数表示是否是最后一条消息
    {
        //1.检测模型是否可用
        if(!isAvailable()){
            ERRO("DeepSeekProvider sendMessageStream failed: model is not available");
            return "";
        }

        //2.构建请求参数
        double temperature = 0.7; //默认温度
        int maxTokens = 2048; //默认最大token数
        if(requestParam.find("temperature")!=requestParam.end()){
            try{
                temperature = std::stod(requestParam.at("temperature"));
            }catch(const std::exception& e){
                ERRO("DeepSeekProvider sendMessageStream failed: invalid temperature value: {}, error: {}", requestParam.at("temperature"), e.what());
            }
        }
        if(requestParam.find("maxTokens")!=requestParam.end()){
            try{
                maxTokens = std::stoi(requestParam.at("maxTokens"));
            }catch(const std::exception& e){
                ERRO("DeepSeekProvider sendMessageStream failed: invalid maxTokens value: {}, error: {}", requestParam.at("maxTokens"), e.what());
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
        requestBody["stream"] = true; //开启流式返回

        //5.序列化
        Json::StreamWriterBuilder writer;
        writer["indentation"] = ""; //去除默认的缩进和换行，使输出更紧凑
        std::string requestBodyStr = Json::writeString(writer, requestBody);
        INFO("DeepSeekProvider sendMessageStream request body: {}", requestBodyStr);

        //6.构造HTTPS客户端
        httplib::Client cli(_endpoint.c_str());
        cli.set_connection_timeout(30,0);   //设置连接超时时间为30秒
        cli.set_read_timeout(300,0);          //流式返回时,可能需要更长的读取超时时间，这里设置为300秒

        //7.构造请求头
        httplib::Headers headers = {
            {"Content-Type", "application/json"},
            {"Authorization", "Bearer " + _apiKey},
            {"Accept", "application/event-stream"} //流式返回通常使用这个Accept头
        };

        //8.流式处理变量
        std::string buffer;             //用于缓存接收到的数据
        bool gotError=false;            //标记响应是否成功
        std::string errorMsg;           //错误信息
        int statusCode=0;               //HTTP状态码
        bool streamFinished=false;      //标记流式响应是否结束
        std::string responseContent;    //最终的响应内容

        //9.创建请求对象
        httplib::Request req;
        req.method = "POST";
        req.path = "/v1/chat/completions";
        req.headers = headers;
        req.body = requestBodyStr;

        //10.设置响应处理器
        req.response_handler = [&](const httplib::Response& res) {
            statusCode = res.status;
            if (res.status != 200) {
                gotError = true;
                errorMsg = "server returned non-200 status code: " + std::to_string(res.status);
                ERRO("DeepSeekProvider sendMessageStream failed: {}", errorMsg);
                return false;//终止请求
            }
            return true;//响应成功，开始接受流式数据
        };

        //11.设置内容接收器-处理服务器返回的流式数据
        req.content_receiver = [&](const char* data, size_t data_length, size_t offset, size_t total_length) {
            //验证响应头是否错误，出错不就继续接受数据了。
            if(gotError){
                return false;//终止请求
            }

            //将接收到的数据追加到缓冲区
            buffer.append(data, data_length);
            INFO("DeepSeekProvider sendMessageStream buffer: {}",buffer);
            
            //处理所有的流式响应的数据块，数据块之间是一个\n\n分隔符
            size_t pos;
            while ((pos = buffer.find("\n\n")) != std::string::npos) {
                std::string chunk = buffer.substr(0, pos);//获取一个完整的数据块
                buffer.erase(0, pos + 2); //移除已处理的数据块和分隔符
                
                //解析该块响应数据中模型返回的有效数据
                //处理空行和注释，注意，以:开头的行是注释行，需要忽略
                if (chunk.empty() || chunk[0] == ':') {
                    continue; //跳过空行和注释行
                }

                //获取模型返回的增量数据，增量数据以"data: "开头
                const std::string dataPrefix = "data: ";
                if (chunk.compare(0, dataPrefix.size(), dataPrefix) == 0) {
                    std::string data = chunk.substr(dataPrefix.size()); //提取增量数据

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
                    //确认content字段存在且有效
                    if (!dataJson.isMember("choices") || !dataJson["choices"].isArray() || dataJson["choices"].empty() || !dataJson["choices"][0].isMember("delta")||!dataJson["choices"][0]["delta"].isMember("content")) {
                        WARN("DeepSeekProvider sendMessageStream failed: incremental data does not contain valid choices array with delta field");
                        continue; //跳过这个数据块，继续处理后续的数据块
                    }
                    std::string incrementalContent = dataJson["choices"][0]["delta"]["content"].asString();
                    responseContent += incrementalContent;//将增量内容追加到最终响应内容中

                    //调用回调函数，将增量内容传递给用户
                    callback(incrementalContent,false);//false表示这不是最后一条消息
                }
            }
            return true;//继续接受数据
        };

        //12.发送请求
        auto result = cli.send(req);
        if (!result) {
            ERRO("DeepSeekProvider sendMessageStream failed: no response from server, error code: {}", httplib::to_string(result.error()));
            return "";
        }
        //确保流式响应已经正常结束了，才返回最终的响应内容
        if(!streamFinished){
            WARN("DeepSeekProvider sendMessageStream failed: stream finished flag is not set, but response has ended");
            return "";
        }

        return responseContent; 
    }

}//end of namespace ai_chat_sdk