#include "../include/SessionManager.h"
#include <iomanip>
#include <sstream>

namespace ai_chat_sdk
{
    // 生成会话ID,格式为"session_时间戳_会话计数"
    std::string SessionManager::generateSessionId()
    {
        // 会话计数自增，确保每个会话ID唯一
        _sessionIdCounter.fetch_add(1);
        std::time_t time = std::time(nullptr); // 获取当前时间戳

        // 开始生成会话ID
        std::ostringstream oss;
        oss << "session_" << time << "_" << std::setw(10) << std::setfill('0') << _sessionIdCounter;
        return oss.str();
    }

    // 生成消息ID,格式为"msg_时间戳_消息计数"
    std::string SessionManager::generateMessageId(size_t messageCounter)
    {
        messageCounter++;                      // 消息计数自增，确保每个消息ID唯一
        std::time_t time = std::time(nullptr); // 获取当前时间戳
        std::ostringstream oss;
        oss << "msg_" << time << "_" << std::setw(10) << std::setfill('0') << messageCounter;
        return oss.str();
    }

    // 创建会话，提供模型名称
    std::string SessionManager::createSession(const std::string &modelName)
    {
        std::lock_guard<std::mutex> lock(_sessionMutex);
        std::string sessionId = generateSessionId();   // 生成唯一会话ID
        Session newSession(modelName);                 // 创建新的会话对象
        newSession._sessionId = sessionId;             // 设置会话ID
        newSession._createdAt = std::time(nullptr);    // 设置会话创建时间
        newSession._updatedAt = newSession._createdAt; // 初始化更新时间为创建时间
        _sessions[sessionId] = newSession;             // 将新会话添加到会话管理器中
        return sessionId;
    }

    // 根据会话ID获取会话信息
    std::shared_ptr<Session> SessionManager::getSession(const std::string &sessionId) const
    {
        std::lock_guard<std::mutex> lock(_sessionMutex);
        auto it = _sessions.find(sessionId);
        if (it != _sessions.end())
        {
            return std::make_shared<Session>(it->second); // 返回会话信息的副本，避免外部修改内部数据
        }
        return nullptr; // 未找到会话返回空指针
    }

    // 往某个会话中添加消息
    bool SessionManager::addMessageToSession(const std::string &sessionId, const Message &message)
    {
        std::lock_guard<std::mutex> lock(_sessionMutex);
        auto it = _sessions.find(sessionId);
        if (it != _sessions.end())
        {
            it->second._messages.push_back(message);    // 将消息添加到会话的消息列表中
            it->second._updatedAt = std::time(nullptr); // 更新会话的更新时间戳
            return true;
        }
        return false; // 未找到会话返回失败
    }

    // 获取某个会话的历史消息
    std::vector<Message> SessionManager::getSessionMessages(const std::string &sessionId) const
    {
        std::lock_guard<std::mutex> lock(_sessionMutex);
        auto it = _sessions.find(sessionId);
        if (it != _sessions.end())
        {
            return it->second._messages; // 返回会话的消息列表副本，避免外部修改内部数据
        }
        return {}; // 未找到会话返回空消息列表
    }

    // 更新会话的时间戳
    bool SessionManager::updateSessionTimestamp(const std::string &sessionId)
    {
        std::lock_guard<std::mutex> lock(_sessionMutex);
        auto it = _sessions.find(sessionId);
        if (it != _sessions.end())
        {
            it->second._updatedAt = std::time(nullptr); // 更新会话的更新时间戳
            return true;
        }
        return false; // 未找到会话返回失败
    }

    // 获取会话列表
    std::vector<std::string> SessionManager::getSessionLists() const
    {
        std::lock_guard<std::mutex> lock(_sessionMutex);
        std::vector<std::string> sessionIds;
        for (const auto &pair : _sessions)
        {
            sessionIds.push_back(pair.first); // 将会话ID添加到列表中
        }
        return sessionIds;
    }

    // 删除会话
    bool SessionManager::deleteSession(const std::string &sessionId)
    {
        std::lock_guard<std::mutex> lock(_sessionMutex);
        return _sessions.erase(sessionId) > 0; // 删除会话并返回是否成功
    }

    // 清空所有会话
    void SessionManager::clearAllSessions()
    {
        std::lock_guard<std::mutex> lock(_sessionMutex);
        _sessions.clear(); // 清空所有会话
    }

    // 获取会话总数
    int64_t SessionManager::getSessionCount() const
    {
        std::lock_guard<std::mutex> lock(_sessionMutex);
        return _sessions.size(); // 返回当前会话总数
    }
} // end of namespace ai_chat_sdk