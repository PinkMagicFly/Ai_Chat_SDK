#pragma once
#include <httplib.h>
#include <memory>
#include <ai_chat_SDK/ChatSDK.h>
#include <ai_chat_SDK/util/myLog.h>
#include <atomic>
#include <thread>
#include <jsoncpp/json/json.h>

namespace ai_chat_server
{
    // 服务器配置信息
    struct ServerConfig
    {
        std::string host = "0.0.0.0";  // 监听地址
        int port = 8807;               // 监听端口
        std::string logLevel = "INFO"; // 日志级别

        // 模型需要的信息
        double temperature = 0.7; // 生成文本的随机程度，值越大越随机，建议在0.7-1.0之间
        int maxTokens = 2048;     // 生成文本的最大长度，单位为token，建议根据实际需求设置，过大可能导致响应时间过长或内存不足

        // API Key;
        std::string deepseekAPIKEY;   // 模型APIKey，必填项，实际通过环境变量赋值即可
        std::string openrouterAPIKEY; // 模型APIKey，必填项，实际通过环境变量赋值即可

        // Ollama
        std::string ollamaModelName; // Ollama模型名称，必填项
        std::string ollamaModelDesc; // Ollama模型描述信息，选填项，可以提供一些关于模型的额外信息，如训练数据、适用场景等，便于用户了解模型特点
        std::string ollamaEndpoint;  // Ollama服务器地址，选填项，本机可以直接填http://localhost:11434，如果Ollama服务器部署在其他地址或端口，需要修改此项
    };

    // 聊天服务器类
    class ChatServer
    {
    public:
        ChatServer(const ServerConfig &config);
        ~ChatServer();

        bool start();           // 启动服务器
        void stop();            // 停止服务器
        bool isRunning() const; // 检查服务器是否正在运行
    private:
        //构建错误响应
        std::string buildErrorResponse(const std::string &message);
        //处理创建会话的请求
        void handleCreateSession(const httplib::Request &req, httplib::Response &res);
        //处理获取会话列表的请求
        void handleGetSessions(const httplib::Request &req, httplib::Response &res);
        //处理获取模型列表的请求
        void handleGetModels(const httplib::Request &req, httplib::Response &res);
        //处理删除会话的请求
        void handleDeleteSession(const httplib::Request &req, httplib::Response &res);
        //处理获取历史消息的请求
        void handleGetMessages(const httplib::Request &req, httplib::Response &res);
        //处理发送消息的请求-全量返回
        void handleSendMessage(const httplib::Request &req, httplib::Response &res);
        //处理发送消息的请求-增量返回
        void handleSendMessageStream(const httplib::Request &req, httplib::Response &res);


    private:
        std::unique_ptr<httplib::Server> _chatServer = nullptr;    // HTTP服务器实例
        std::shared_ptr<ai_chat_sdk::ChatSDK> _chat_sdk = nullptr; // ChatSDK实例
        std::atomic<bool> _isRunning = {false};                    // 服务器运行状态
        ServerConfig _config;                                      // 服务器配置信息
    };
} // namespace ai_chat_server