#include "../include/SessionManager.h"
#include <iomanip>
#include <sstream>
#include "../include/util/myLog.h"

namespace ai_chat_sdk
{
    // 构造函数，初始化数据管理器
    SessionManager::SessionManager(const std::string &dbName) : _dataManager(dbName)
    {
        // 从数据库加载会话数据到内存
        auto sessionsFromDb = _dataManager.getAllSessions();
        for (const auto &session : sessionsFromDb)
        {
            _sessions[session->_sessionId] = session; // 将数据库中的会话加载到内存中
            // 会话消息需要时再获取取，避免一次性加载过多数据占用内存
        }
    }

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
        auto newSession = std::make_shared<Session>(modelName); // 创建新的会话对象
        std::string sessionId;
        {
            std::lock_guard<std::mutex> lock(_sessionMutex); // 加锁保护会话数据的线程安全
            sessionId = generateSessionId();                 // 生成唯一会话ID
            newSession->_sessionId = sessionId;              // 设置会话ID
            newSession->_createdAt = std::time(nullptr);     // 设置会话创建时间
            newSession->_updatedAt = newSession->_createdAt; // 初始化更新时间为创建时间
            _sessions[sessionId] = newSession;               // 将新会话添加到会话管理器中
        }
        _dataManager.insertSession(*newSession); // 将新会话插入数据库，注意数据库操作自有一把锁，这里不要把锁嵌套进去
        return sessionId;
    }

    // 根据会话ID获取会话信息
    Session SessionManager::getSession(const std::string &sessionId)
    {
        // 先在内存里找
        _sessionMutex.lock(); // 加锁保护会话数据的线程安全
        auto it = _sessions.find(sessionId);
        if (it != _sessions.end())
        {
            it->second->_messages = _dataManager.queryMessages(sessionId); // 从数据库查询会话的消息列表，并更新内存中的会话对象
            auto result = *(it->second);                                   // 获取会话对象的副本，避免外部修改内部数据
            // 如果内存里找到了会话对象，直接返回
            _sessionMutex.unlock(); // 解锁
            return result; // 返回找到的会话对象
        }
        _sessionMutex.unlock(); // 解锁
        // 内存里没有找到会话对象，再去数据库里找
        auto sessionFromDb = _dataManager.querySession(sessionId);
        {
            std::lock_guard<std::mutex> lock(_sessionMutex); // 加锁保护会话数据的线程安全
            if (sessionFromDb)
            {
                // 如果数据库里找到了会话对象，先把它加载到内存里，再返回
                // 再次确定内存里没有这个会话对象，避免重复加载
                if (_sessions.find(sessionId) == _sessions.end())
                {
                    _sessions[sessionId] = sessionFromDb; // 将数据库中的会话加载到内存中
                }
                auto result = *(sessionFromDb); // 获取会话对象的副本，避免外部修改内部数据
                return result;                  // 返回找到的会话对象
            }
            WARN("Session not found for sessionId: {}", sessionId);
            return Session(""); // 未找到会话返回空会话对象
        }
    }

    // 往某个会话中添加消息
    bool SessionManager::addMessageToSession(const std::string &sessionId, const Message &message)
    {
        _sessionMutex.lock(); // 加锁保护会话数据的线程安全
        auto it = _sessions.find(sessionId);
        if (it != _sessions.end())
        {

            Message newMessage(message._role, message._content);                     // 创建消息
            newMessage._messageid = generateMessageId(it->second->_messages.size()); // 生成唯一消息ID
            it->second->_messages.push_back(newMessage);                             // 将消息添加到会话的消息列表中
            it->second->_updatedAt = std::time(nullptr);                             // 更新会话的更新时间戳
            INFO("Message added to sessionId {}: content={}", sessionId, newMessage._content);
            _sessionMutex.unlock();                            // 解锁
            _dataManager.insertMessage(sessionId, newMessage); // 将消息插入数据库，注意数据库操作自有一把锁，这里不要把锁嵌套进去
            // 会话时间戳已经在插入操作里进行更新了，这里就不需要再调用一次更新会话时间戳的接口了
            return true;
        }
        _sessionMutex.unlock(); // 解锁
        WARN("Failed to add message to sessionId {}: session not found", sessionId);
        return false; // 未找到会话返回失败
    }

    // 获取某个会话的历史消息
    std::vector<Message> SessionManager::getSessionMessages(const std::string &sessionId) const
    {
        _sessionMutex.lock(); // 加锁保护会话数据的线程安全
        // 先在内存里找会话对象，获取它的消息列表
        auto it = _sessions.find(sessionId);
        if (it != _sessions.end())
        {
            auto result = it->second->_messages; // 获取会话的消息列表副本，避免外部修改内部数据
            _sessionMutex.unlock();              // 解锁
            return result;                       // 返回会话的消息列表副本，避免外部修改内部数据
        }
        // 内存里没有找到会话对象，再去数据库里找会话的消息列表
        _sessionMutex.unlock(); // 解锁
        auto sessionFromDb = _dataManager.querySession(sessionId);
        if (sessionFromDb)
        {
            return sessionFromDb->_messages; // 返回数据库中会话的消息列表副本，避免外部修改内部数据
        }
        WARN("Session not found for sessionId: {}, cannot get messages", sessionId);
        return {}; // 未找到会话返回空消息列表
    }

    // 更新会话的时间戳
    bool SessionManager::updateSessionTimestamp(const std::string &sessionId)
    {
        _sessionMutex.lock(); // 加锁保护会话数据的线程安全
        auto it = _sessions.find(sessionId);
        if (it != _sessions.end())
        {
            it->second->_updatedAt = std::time(nullptr);               // 更新会话的更新时间戳
            auto updatedAt = it->second->_updatedAt;                   // 获取更新后的时间戳
            _sessionMutex.unlock();                                    // 解锁
            _dataManager.updateSessionTimestamp(sessionId, updatedAt); // 将更新后的时间戳同步到数据库，注意数据库操作自有一把锁，这里不要把锁嵌套进去
            return true;
        }
        _sessionMutex.unlock(); // 解锁
        WARN("Failed to update session timestamp for sessionId {}: session not found", sessionId);
        return false; // 未找到会话返回失败
    }

    // 获取会话列表
    std::vector<std::string> SessionManager::getSessionLists() const
    {
        _sessionMutex.lock(); // 加锁保护会话数据的线程安全

        // 创建临时对话ID列表，目的是将会话列表按会话更新时间降序排序
        std::vector<std::pair<std::time_t, std::shared_ptr<Session>>> sessionPairs;
        sessionPairs.reserve(_sessions.size());
        auto _sessionsCopy = _sessions; // 复制当前内存中的会话数据，避免在排序过程中修改原数据导致线程安全问题
        // 将内存中的会话对象和数据库中的会话对象合并，确保获取到最新的会话列表
        for (const auto &pair : _sessions)
        {
            sessionPairs.emplace_back(pair.second->_updatedAt, pair.second); // 将会话的更新时间和会话对象一起存储到临时列表中
        }
        _sessionMutex.unlock();                              // 解锁
        auto sessionsFromDb = _dataManager.getAllSessions(); // 从数据库获取所有会话对象
        for (const auto &sessionFromDb : sessionsFromDb)
        {
            // 如果内存中已经有这个会话对象了，就不需要再添加一次了，避免重复
            if (_sessionsCopy.find(sessionFromDb->_sessionId) == _sessionsCopy.end())
            {
                sessionPairs.emplace_back(sessionFromDb->_updatedAt, sessionFromDb); // 将数据库中的会话对象添加到临时列表中
            }
        }
        // 按照会话更新时间降序排序
        std::sort(sessionPairs.begin(), sessionPairs.end(), [](const auto &a, const auto &b)
                  {
                      return a.first > b.first; // 按照更新时间降序排序
                  });

        // 从排序后的临时列表中提取会话ID，生成最终的会话ID列表
        std::vector<std::string> sessionIds;
        for (const auto &pair : sessionPairs)
        {
            sessionIds.push_back(pair.second->_sessionId); // 将会话ID添加到列表中
        }
        return sessionIds;
    }

    // 删除会话
    bool SessionManager::deleteSession(const std::string &sessionId)
    {
        _sessionMutex.lock(); // 加锁保护会话数据的线程安全
        auto it = _sessions.find(sessionId);
        if (it != _sessions.end())
        {
            _sessions.erase(it);                   // 从会话管理器中删除会话
            _sessionMutex.unlock();                // 解锁
            _dataManager.deleteSession(sessionId); // 从数据库中删除会话，注意数据库操作自有一把锁，这里不要把锁嵌套进去
            return true;
        }
        _sessionMutex.unlock(); // 解锁
        return false;           // 未找到会话返回失败
    }

    // 清空所有会话
    void SessionManager::clearAllSessions()
    {
        _sessionMutex.lock();             // 加锁保护会话数据的线程安全
        _sessions.clear();                // 清空所有会话
        _sessionMutex.unlock();           // 解锁
        _dataManager.deleteAllSessions(); // 从数据库中删除所有会话，注意数据库操作自有一把锁，这里不要把锁嵌套进去
    }

    // 获取会话总数
    int64_t SessionManager::getSessionCount() const
    {
        std::lock_guard<std::mutex> lock(_sessionMutex);
        return _sessions.size(); // 返回当前会话总数
    }
} // end of namespace ai_chat_sdk