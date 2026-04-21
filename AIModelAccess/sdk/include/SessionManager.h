#pragma once
#include"common.h"
#include<mutex>
#include<atomic>
#include<memory>
#include<unordered_map>

namespace ai_chat_sdk
{
    class SessionManager
    {
    public:
        //创建会话，提供模型名称
        std::string createSession(const std::string &modelName);
        //根据会话ID获取会话信息
        std::shared_ptr<Session> getSession(const std::string &sessionId) const;
        //往某个会话中添加消息
        bool addMessageToSession(const std::string &sessionId, const Message &message);
        //获取某个会话的历史消息
        std::vector<Message> getSessionMessages(const std::string &sessionId) const;
        //更新会话的时间戳
        bool updateSessionTimestamp(const std::string &sessionId);
        //获取会话列表
        std::vector<std::string> getSessionLists() const;
        //删除会话
        bool deleteSession(const std::string &sessionId);
        //清空所有会话
        void clearAllSessions();
        //获取会话总数
        int64_t getSessionCount() const;
    
    private:
        //生成唯一会话ID
        std::string generateSessionId();
        //生成消息ID
        std::string generateMessageId(size_t messageCounter);

    private:
        //管理所有会话key:会话ID，value:会话信息
        std::unordered_map<std::string,Session> _sessions;
        mutable std::mutex _sessionMutex; //保护会话数据的线程安全,支持const方法修改
        std::atomic<int64_t> _sessionIdCounter={0}; //记录会话总数,用于生成唯一会话ID
    };
} // end of namespace ai_chat_sdk