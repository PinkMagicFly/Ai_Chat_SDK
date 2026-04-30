#include "ChatServer.h"

namespace ai_chat_server
{
    ChatServer::ChatServer(const ServerConfig &config)
    {
        // 初始化ChatSDK，注册模型提供者，并初始化模型
        // deepseek-chat
        auto deepseekConfig = std::make_shared<ai_chat_sdk::APIConfig>();
        deepseekConfig->_modelName = "deepseek-chat";
        deepseekConfig->_apiKey = config.deepseekAPIKEY;
        deepseekConfig->_temperature = config.temperature;
        deepseekConfig->_max_tokens = config.maxTokens;
        // openai/gpt-4o-mini
        auto chatGPTConfig = std::make_shared<ai_chat_sdk::APIConfig>();
        chatGPTConfig->_modelName = "openai/gpt-4o-mini";
        chatGPTConfig->_apiKey = config.openrouterAPIKEY;
        chatGPTConfig->_temperature = config.temperature;
        chatGPTConfig->_max_tokens = config.maxTokens;
        // google/gemini-2.0-flash-001
        auto geminiConfig = std::make_shared<ai_chat_sdk::APIConfig>();
        geminiConfig->_modelName = "google/gemini-2.0-flash-001";
        geminiConfig->_apiKey = config.openrouterAPIKEY;
        geminiConfig->_temperature = config.temperature;
        geminiConfig->_max_tokens = config.maxTokens;
        // Ollama中的gemma3:270m模型
        auto ollamaConfig = std::make_shared<ai_chat_sdk::OllamaConfig>();
        ollamaConfig->_modelName = config.ollamaModelName;
        ollamaConfig->_endpoint = config.ollamaEndpoint;
        ollamaConfig->_modelDesc = config.ollamaModelDesc;
        ollamaConfig->_temperature = config.temperature;
        ollamaConfig->_max_tokens = config.maxTokens;
        // 把所有模型配置放到一个vector中，传给ChatSDK进行初始化
        std::vector<std::shared_ptr<ai_chat_sdk::Config>> configs = {deepseekConfig, chatGPTConfig, geminiConfig, ollamaConfig};
        // 保持服务器配置信息
        _config = config;

        // 初始化ChatSDK
        _chat_sdk = std::make_shared<ai_chat_sdk::ChatSDK>();
        if (!_chat_sdk->initializeSDK(configs))
        {
            ERRO("Failed to initialize ChatSDK");
            return;
        }

        // 创建HTTP服务器
        _chatServer = std::make_unique<httplib::Server>();
        if (!_chatServer)
        {
            ERRO("Failed to create HTTP server");
            return;
        }
    }

    // 检查服务器是否正在运行
    bool ChatServer::isRunning() const
    {
        return _isRunning.load();
    }

    // 启动服务器
    bool ChatServer::start()
    {
        if (_isRunning.load())
        {
            ERRO("Server is already running");
            return false;
        }
        // 设置HTTP路由规则
        setupRoutes();

        //默认页面是index.html，所以我们在www目录里放一个index.html就可以了，当然你也可以放其他的静态资源了，比如css、js、图片等，然后在index.html里引用这些资源就可以了
        // set_base_dir方法可以设置一个静态文件目录，服务器会自动处理对这个目录下的文件的请求了，
        //比如访问http://host:port/index.html就会返回www目录下的index.html文件了，这样就可以提供一个简单的界面了，方便测试了
        //注意，httplib中不加index.html的话，默认也是会访问index.html的，所以访问http://host:port/也是可以访问到index.html的了
        _chatServer->set_base_dir("./www");

        // 为了不卡服务器主线程，这里启动一个新的线程来运行http服务
        std::thread serverThread([this]()
                                 {
            _chatServer->listen(_config.host, _config.port);
            INFO("Server started at {}:{}", _config.host, _config.port); });
        serverThread.detach(); // 分离线程，让它在后台运行

        _isRunning.store(true);
        return true;
    }

    // 停止服务器
    void ChatServer::stop()
    {
        if (!_isRunning.load())
        {
            ERRO("Server is not running");
            return;
        }
        if (_chatServer)
        {
            _chatServer->stop();
        }
        _isRunning.store(false);
        INFO("Server stopped");
    }

    // 构建响应
    std::string ChatServer::buildResponse(const std::string &message, bool success)
    {
        Json::Value resJson;
        resJson["success"] = success;
        resJson["message"] = message;
        // 序列化响应体，返回给客户端
        Json::StreamWriterBuilder writer;
        writer["indentation"] = ""; // 去除默认的缩进和换行，使输出更紧凑
        return Json::writeString(writer, resJson);
    }

    // 处理创建会话的请求
    void ChatServer::handleCreateSession(const httplib::Request &req, httplib::Response &res)
    {
        // 获取请求参数，请求参数在请求体里
        // 反序列化请求体，获取请求体的json数据
        Json::Value reqJson;
        Json::CharReaderBuilder reader;
        std::string errs;
        std::istringstream ss(req.body);
        // 解析请求体，如果解析失败了，说明请求体有问题，返回400 Bad Request，并告诉客户端请求体有问题
        if (!Json::parseFromStream(reader, ss, &reqJson, &errs))
        {
            // 填充响应体，告诉客户端请求体有问题，解析失败了
            std::string errorMessage = "Failed to parse request body: " + errs;
            std::string errorResponse = buildResponse(errorMessage, false);
            res.status = 400; // Bad Request
            res.set_content(errorResponse, "application/json");
            return;
        }

        // 获取请求参数
        std::string modelName = reqJson["model"].asString();
        if (modelName.empty())
        { // 填充响应体，告诉客户端请求参数有问题，model参数不能为空
            std::string errorMessage = "Model name is required";
            std::string errorResponse = buildResponse(errorMessage, false);
            res.status = 400; // Bad Request
            res.set_content(errorResponse, "application/json");
            return;
        }

        // 创建会话
        std::string sessionId = _chat_sdk->createSession(modelName);
        if (sessionId.empty())
        {
            // 填充响应体，告诉客户端创建会话失败了
            std::string errorMessage = "Failed to create session with model: " + modelName;
            std::string errorResponse = buildResponse(errorMessage, false);
            res.status = 500; // Internal Server Error
            res.set_content(errorResponse, "application/json");
            return;
        }

        // 构建响应体
        Json::Value resJson;
        resJson["success"] = true;
        resJson["message"] = "Session created successfully";
        resJson["data"] = Json::objectValue;
        resJson["data"]["session_id"] = sessionId;
        resJson["data"]["model"] = modelName;

        // 序列化响应体，返回给客户端
        Json::StreamWriterBuilder writer;
        writer["indentation"] = ""; // 去除默认的缩进和换行，使输出更紧凑
        res.status = 200;           // OK
        res.set_content(Json::writeString(writer, resJson), "application/json");
        return;
    }

    // 处理获取会话列表的请求
    void ChatServer::handleGetSessions(const httplib::Request &req, httplib::Response &res)
    {
        // 获取所有会话列表
        std::vector<std::string> session_Ids = _chat_sdk->getSessionLists();

        // 构建session信息
        Json::Value sessionsJson(Json::arrayValue);
        for (const auto &sessionId : session_Ids)
        {
            // 获取会话信息
            auto sessionInfo = _chat_sdk->getSession(sessionId);
            if (sessionInfo == nullptr)
            {
                WARN("Failed to get session info for session id: {}", sessionId);
                continue; // 获取会话信息失败了，跳过这个会话，继续处理后续的会话
            }

            Json::Value sessionJson;
            sessionJson["id"] = sessionId;
            sessionJson["model"] = sessionInfo->_modelName;
            sessionJson["created_at"] = (Json::Int64)sessionInfo->_createdAt;
            sessionJson["updated_at"] = (Json::Int64)sessionInfo->_updatedAt;
            sessionJson["message_count"] = (Json::Int)sessionInfo->_messages.size();
            sessionJson["first_user_message"] = sessionInfo->_messages.empty() ? "" : sessionInfo->_messages.front()._content;
            sessionsJson.append(sessionJson);
        }

        // 构建响应体
        Json::Value resJson;
        resJson["success"] = true;
        resJson["message"] = "Sessions retrieved successfully";
        resJson["data"] = sessionsJson;

        // 序列化响应体，返回给客户端
        Json::StreamWriterBuilder writer;
        writer["indentation"] = ""; // 去除默认的缩进和换行，使输出更紧凑
        res.status = 200;           // OK
        res.set_content(Json::writeString(writer, resJson), "application/json");
    }
    // 处理获取模型列表的请求
    void ChatServer::handleGetModels(const httplib::Request &req, httplib::Response &res)
    {
        // 获取所有模型列表
        std::vector<ai_chat_sdk::LLMInfo> models = _chat_sdk->getAvailableModels();

        // 构建响应体
        Json::Value resJson;
        resJson["success"] = true;
        resJson["message"] = "Models retrieved successfully";
        Json::Value modelsJson(Json::arrayValue);
        for (const auto &model : models)
        {
            Json::Value modelJson;
            modelJson["name"] = model._modelName;
            modelJson["desc"] = model._description;
            modelsJson.append(modelJson);
        }
        resJson["data"] = modelsJson;

        // 序列化响应体，返回给客户端
        Json::StreamWriterBuilder writer;
        writer["indentation"] = ""; // 去除默认的缩进和换行，使输出更紧凑
        res.status = 200;           // OK
        res.set_content(Json::writeString(writer, resJson), "application/json");
    }

    // 处理删除会话的请求
    void ChatServer::handleDeleteSession(const httplib::Request &req, httplib::Response &res)
    {
        // 获取会话id，会话id是一个路径参数
        std::string sessionId = req.matches[1]; // 路径参数在httplib里是通过正则表达式匹配的，匹配到的参数会保存在matches数组里，matches[0]是整个匹配的字符串，matches[1]是第一个括号匹配的字符串，也就是会话id
        if (sessionId.empty())
        {
            // 填充响应体，告诉客户端会话id不能为空
            std::string errorMessage = "Session ID is required";
            std::string errorResponse = buildResponse(errorMessage, false);
            res.status = 400; // Bad Request
            res.set_content(errorResponse, "application/json");
            return;
        }

        // 删除会话
        bool deleteResult = _chat_sdk->deleteSession(sessionId);
        if (!deleteResult)
        {
            // 填充响应体，告诉客户端删除会话失败了，可能是会话id不存在了
            std::string errorMessage = "Failed to delete session with id: " + sessionId;
            std::string errorResponse = buildResponse(errorMessage, false);
            res.status = 404; // Not Found
            res.set_content(errorResponse, "application/json");
            return;
        }

        // 构建响应体
        std::string successMessage = "Session deleted successfully";
        std::string successResponse = buildResponse(successMessage, true);
        res.status = 200; // OK
        res.set_content(successResponse, "application/json");
        return;
    }

    // 处理获取历史消息的请求
    void ChatServer::handleGetMessages(const httplib::Request &req, httplib::Response &res)
    {
        // 获取会话id，会话id是一个路径参数
        std::string sessionId = req.matches[1]; // 路径参数在httplib里是通过正则表达式匹配的，匹配到的参数会保存在matches数组里，matches[0]是整个匹配的字符串，matches[1]是第一个括号匹配的字符串，也就是会话id
        if (sessionId.empty())
        {
            // 填充响应体，告诉客户端会话id不能为空
            std::string errorMessage = "Session ID is required";
            std::string errorResponse = buildResponse(errorMessage, false);
            res.status = 400; // Bad Request
            res.set_content(errorResponse, "application/json");
            return;
        }

        // 获取会话信息
        auto sessionInfo = _chat_sdk->getSession(sessionId);
        if (sessionInfo == nullptr)
        {
            // 填充响应体，告诉客户端会话id不存在了
            std::string errorMessage = "Session not found with id: " + sessionId;
            std::string errorResponse = buildResponse(errorMessage, false);
            res.status = 404; // Not Found
            res.set_content(errorResponse, "application/json");
            return;
        }

        // 构建响应体
        Json::Value resJson;
        resJson["success"] = true;
        resJson["message"] = "Messages retrieved successfully";
        Json::Value messagesJson(Json::arrayValue);
        for (const auto &message : sessionInfo->_messages)
        {
            Json::Value messageJson;
            messageJson["role"] = message._role;
            messageJson["content"] = message._content;
            messageJson["timestamp"] = (Json::Int64)message._timestamp;
            messageJson["id"] = message._messageid;
            messagesJson.append(messageJson);
        }
        resJson["data"] = messagesJson;

        // 序列化响应体，返回给客户端
        Json::StreamWriterBuilder writer;
        writer["indentation"] = ""; // 去除默认的缩进和换行，使输出更紧凑
        res.status = 200;           // OK
        res.set_content(Json::writeString(writer, resJson), "application/json");
        return;
    }

    // 处理发送消息的请求-全量返回
    void ChatServer::handleSendMessage(const httplib::Request &req, httplib::Response &res)
    {
        // 获取请求参数，请求体中的json格式为session_id和message两个字段，session_id是必填项，message是必填项
        Json::Value reqJson;
        Json::CharReaderBuilder reader;
        std::string errs;
        std::istringstream ss(req.body);
        // 解析请求体，如果解析失败了，说明请求体有问题，返回400 Bad Request，并告诉客户端请求体有问题
        if (!Json::parseFromStream(reader, ss, &reqJson, &errs))
        {
            // 填充响应体，告诉客户端请求体有问题，解析失败了
            std::string errorMessage = "Failed to parse request body: " + errs;
            std::string errorResponse = buildResponse(errorMessage, false);
            res.status = 400; // Bad Request
            res.set_content(errorResponse, "application/json");
            return;
        }
        // 获取请求参数
        std::string sessionId = reqJson["session_id"].asString();
        std::string message = reqJson["message"].asString();
        if (sessionId.empty() || message.empty())
        { // 填充响应体，告诉客户端请求参数有问题，session_id和message参数不能为空
            std::string errorMessage = "Session ID and message are required";
            std::string errorResponse = buildResponse(errorMessage, false);
            res.status = 400; // Bad Request
            res.set_content(errorResponse, "application/json");
            return;
        }

        // 发送消息，获取模型的回复
        std::string reply = _chat_sdk->sendMessage(sessionId, message);
        if (reply.empty())
        {
            // 填充响应体，告诉客户端模型回复消息失败了
            std::string errorMessage = "Failed to get reply from session: " + sessionId;
            std::string errorResponse = buildResponse(errorMessage, false);
            res.status = 500; // Internal Server Error
            res.set_content(errorResponse, "application/json");
            return;
        }

        // 构建响应体
        Json::Value resJson;
        resJson["success"] = true;
        resJson["message"] = "Message sent successfully";
        resJson["data"] = Json::objectValue;
        resJson["data"]["response"] = reply;
        resJson["data"]["session_id"] = sessionId;

        // 序列化响应体，返回给客户端
        Json::StreamWriterBuilder writer;
        writer["indentation"] = ""; // 去除默认的缩进和换行，使输出更紧凑
        res.status = 200;           // OK
        res.set_content(Json::writeString(writer, resJson), "application/json");
        return;
    }

    // 处理发送消息的请求-增量返回
    void ChatServer::handleSendMessageStream(const httplib::Request &req, httplib::Response &res)
    {
        // 获取请求参数，和handleSendMessage一样
        Json::Value reqJson;
        Json::CharReaderBuilder reader;
        std::string errs;
        std::istringstream ss(req.body);
        if (!Json::parseFromStream(reader, ss, &reqJson, &errs))
        {
            std::string errorMessage = "Failed to parse request body: " + errs;
            std::string errorResponse = buildResponse(errorMessage, false);
            res.status = 400; // Bad Request
            res.set_content(errorResponse, "application/json");
            return;
        }

        std::string sessionId = reqJson["session_id"].asString();
        std::string message = reqJson["message"].asString();
        if (sessionId.empty() || message.empty())
        {
            std::string errorMessage = "Session ID and message are required";
            std::string errorResponse = buildResponse(errorMessage, false);
            res.status = 400; // Bad Request
            res.set_content(errorResponse, "application/json");
            return;
        }

        // 准备流式响应
        res.status = 200;                            // OK
        res.set_header("Cache-Control", "no-cache"); // 禁止缓存
        res.set_header("Connection", "keep-alive");  // 保持长连接，允许服务器持续发送数据
        // set_chunked_content_provider方法可以让服务器持续发送数据，参数是一个回调函数，服务器会不断调用这个回调函数来获取要发送的数据，直到回调函数返回false或者连接断开了。
        // 外层的http响应，本来是一直调用这个回调数据，要数据，但我这里内部调用了内层回调，所以只会调用一次外层回调，然后期间内层回调会一直用sink发送数据给客户端，直到sink.done()，
        // 而return true只是表示外层回调这一次调用是否成功，至于外层回调要不要再次调用，则由sink.done()决定，当然返回false就直接终止了。
        res.set_chunked_content_provider("text/event-stream", [this, sessionId, message](size_t offset, httplib::DataSink &sink) -> bool
                                         {
                                             auto writeChunk = [&](const std::string &chunkMessage, bool isFinal = false)
                                             {
                                                 // 将chunk转换为SSE数据格式
                                                 // valueToQuotedString方法可以将字符串转换为JSON字符串格式，并且会自动转义特殊字符，这样就可以保证发送的数据格式正确了
                                                 std::string sseData = "data: " + Json::valueToQuotedString(chunkMessage.c_str()) + "\n\n";
                                                 sink.write(sseData.c_str(), sseData.size()); // 将数据写入响应流，立即发送给客户端，而不是等待缓冲区满了才发送，这样就实现了增量返回的效果
                                                 // 如果isFinal参数为true，说明这是最后一个数据块了，发送一个特殊的结束标志，告诉客户端数据发送完了
                                                 if (isFinal)
                                                 { // 如果是最后一个数据块了，发送一个特殊的结束标志，告诉客户端数据发送完了
                                                     std::string endData = "data: [DONE]\n\n";
                                                     sink.write(endData.c_str(), endData.size());
                                                     sink.done(); // 告诉服务器数据发送完了，可以关闭连接了
                                                 }
                                             };
                                             // 先发送一个空的数据库，避免客户端一直在等待数据，直到超时了，才会触发错误回调函数了
                                             writeChunk("", false);
                                             // 调用ChatSDK的sendMessageStream方法来发送消息，获取模型的回复，sendMessageStream方法会持续调用writeChunk回调函数来发送数据块，直到模型回复完了或者发生错误了
                                             _chat_sdk->sendMessageIncremental(sessionId, message, writeChunk);
                                             return true; // 本次供给成功；流是否结束由 sink.done() 控制
                                         });
    }

    // 设置HTTP路由规则
    void ChatServer::setupRoutes()
    {
        // 设置HTTP路由规则，httplib支持RESTful风格的路由规则了，所以可以直接在路径里定义参数了，不需要自己解析了
        // 创建会话
        _chatServer->Post("/api/session", [this](const httplib::Request &req, httplib::Response &res)
                          { handleCreateSession(req, res); });
        // 获取会话列表
        _chatServer->Get("/api/sessions", [this](const httplib::Request &req, httplib::Response &res)
                         { handleGetSessions(req, res); });
        // 获取模型列表
        _chatServer->Get("/api/models", [this](const httplib::Request &req, httplib::Response &res)
                         { handleGetModels(req, res); });
        // 删除会话，路径参数是会话id
        _chatServer->Delete(R"(/api/session/(\w+))", [this](const httplib::Request &req, httplib::Response &res)
                            { handleDeleteSession(req, res); });
        // 获取历史消息，路径参数是会话id
        _chatServer->Get(R"(/api/session/(\w+)/history)", [this](const httplib::Request &req, httplib::Response &res)
                         { handleGetMessages(req, res); });
        // 发送消息-全量返回
        _chatServer->Post("/api/message", [this](const httplib::Request &req, httplib::Response &res)
                          { handleSendMessage(req, res); });
        // 发送消息-增量返回
        _chatServer->Post("/api/message/async", [this](const httplib::Request &req, httplib::Response &res)
                          { handleSendMessageStream(req, res); });
    }

    ChatServer::~ChatServer()
    {
        // 服务器会在析构时自动停止，无需手动调用stop方法
    }
} // namespace ai_chat_server