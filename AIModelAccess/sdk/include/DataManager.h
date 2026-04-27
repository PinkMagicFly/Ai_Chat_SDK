#pragma once

#include <sqlite3.h>
#include <string>
#include<iostream>
#include <mutex>
#include <memory>
#include <vector>
#include"common.h"


namespace ai_chat_sdk
{

    class DataManager
    {
    public:
        DataManager(const std::string& dbName);
        ~DataManager();
        //Session相关接口
        //添加Session
        bool insertSession(const Session& session);
        //查询指定Session
        std::shared_ptr<Session> querySession(const std::string& sessionId) const;
        //更新指定会话的时间戳
        bool updateSessionTimestamp(const std::string& sessionId, std::time_t timestamp);
        //删除Session,注意删除Session会级联删除相关的Message
        bool deleteSession(const std::string& sessionId);
        //获取所有会话id列表
        std::vector<std::string> getAllSessionIds() const;
        //获取所有会话信息
        std::vector<std::shared_ptr<Session>> getAllSessions() const;
        //获取会话总数
        size_t getSessionCount() const;
//////////////////////////////////////////////////////////////////
        //Message相关接口
        //插入message,注意，插入消息是还要更新会话的时间戳
        bool insertMessage(const std::string& sessionId, const Message& message);
        //获取指定会话的历史消息
        std::vector<Message> queryMessages(const std::string& sessionId) const;
        //删除指定会话的所有消息
        bool deleteMessages(const std::string& sessionId);

    private:
        //初始化数据库
        bool initDB();
        //执行SQL语句
        bool executeSQL(const std::string& sql);

    private:
        sqlite3* _db;
        std::string _dbName;
        mutable std::mutex _db_mutex;

    };

} // namespace ai_chat_sdk