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

    // 构建错误响应
    std::string ChatServer::buildErrorResponse(const std::string &message)
    {
        Json::Value resJson;
        resJson["success"] = false;
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
            std::string errorResponse = buildErrorResponse(errorMessage);
            res.status = 400; // Bad Request
            res.set_content(errorResponse, "application/json");
            return;
        }

        // 获取请求参数
        std::string modelName = reqJson["model"].asString();
        if (modelName.empty())
        { // 填充响应体，告诉客户端请求参数有问题，model参数不能为空
            std::string errorMessage = "Model name is required";
            std::string errorResponse = buildErrorResponse(errorMessage);
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
            std::string errorResponse = buildErrorResponse(errorMessage);
            res.status = 500;           // Internal Server Error
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
        //获取所有会话列表
        std::vector<std::string> session_Ids = _chat_sdk->getSessionLists();

        //构建session信息
        Json::Value sessionsJson(Json::arrayValue);
        for (const auto &sessionId : session_Ids)
        {
            //获取会话信息
            auto sessionInfo = _chat_sdk->getSession(sessionId);
            if(sessionInfo == nullptr){
                WARN("Failed to get session info for session id: {}", sessionId);
                continue; //获取会话信息失败了，跳过这个会话，继续处理后续的会话
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

        //构建响应体
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
        //获取所有模型列表
        std::vector<ai_chat_sdk::LLMInfo> models = _chat_sdk->getAvailableModels();

        //构建响应体
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
    }

    // 处理获取历史消息的请求
    void ChatServer::handleGetMessages(const httplib::Request &req, httplib::Response &res)
    {
    }

    // 处理发送消息的请求-全量返回
    void ChatServer::handleSendMessage(const httplib::Request &req, httplib::Response &res)
    {
    }

    // 处理发送消息的请求-增量返回
    void ChatServer::handleSendMessageStream(const httplib::Request &req, httplib::Response &res)
    {
    }

    ChatServer::~ChatServer()
    {
        // 服务器会在析构时自动停止，无需手动调用stop方法
    }
} // namespace ai_chat_server