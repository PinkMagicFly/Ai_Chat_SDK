#include "../include/DataManager.h"
#include "../include/util/myLog.h"

namespace ai_chat_sdk
{

    DataManager::DataManager(const std::string &dbName) : _db(nullptr), _dbName(dbName)
    {
        // 创建并打开数据库
        int rc = sqlite3_open(_dbName.c_str(), &_db);
        if (rc != SQLITE_OK)
        {
            ERRO("Cannot open database: {}", sqlite3_errmsg(_db));
            _db = nullptr;
            return;
        }
        INFO("Database opened successfully: {}", dbName);

        // 初始化数据库表
        if (!initDB())
        {
            ERRO("Failed to initialize database: {}", dbName);
            sqlite3_close(_db);
            _db = nullptr;
            return;
        }
    }

    DataManager::~DataManager()
    {
        if (_db)
        {
            sqlite3_close(_db);
            INFO("Database closed: {}", _dbName);
        }
    }

    // 初始化数据库
    bool DataManager::initDB()
    {
        // 创建Session表
        std::string createSessionTableSQL = R"(
            CREATE TABLE IF NOT EXISTS Sessions (
                sessionId TEXT PRIMARY KEY,
                modelName TEXT NOT NULL,
                createdAt INTEGER NOT NULL,
                updatedAt INTEGER NOT NULL
            );
        )";
        // 执行创建Session表的SQL语句
        if (!executeSQL(createSessionTableSQL))
        {
            ERRO("Failed to create Session table");
            return false;
        }
        // 创建Message表
        std::string createMessageTableSQL = R"(
            CREATE TABLE IF NOT EXISTS Message (
                messageId TEXT PRIMARY KEY,
                sessionId TEXT NOT NULL,
                role TEXT NOT NULL,
                content TEXT NOT NULL,
                timestamp INTEGER NOT NULL,
                FOREIGN KEY(sessionId) REFERENCES Sessions(sessionId) ON DELETE CASCADE
            );
        )";

        if (!executeSQL(createMessageTableSQL))
        {
            ERRO("Failed to create Message table");
            return false;
        }

        INFO("Database initialized successfully");
        return true;
    }
    // 执行SQL语句
    bool DataManager::executeSQL(const std::string &sql)
    {
        // 检测数据库是否打开
        if (!_db)
        {
            ERRO("Database is not open");
            return false;
        }
        char *errMsg = nullptr;
        std::lock_guard<std::mutex> lock(_db_mutex);
        int rc = sqlite3_exec(_db, sql.c_str(), nullptr, nullptr, &errMsg);
        if (rc != SQLITE_OK)
        {
            ERRO("executeSQL error: {}", errMsg);
            sqlite3_free(errMsg);
            return false;
        }
        return true;
    }

    // 插入会话
    bool DataManager::insertSession(const Session &session)
    {
        
        // 检查会话是否已存在
        std::string checkSQL = "SELECT sessionId FROM Sessions WHERE sessionId = '" + session._sessionId + "';";
        if (!executeSQL(checkSQL))
        {
            ERRO("Failed to check session existence");
            return false;
        }
        std::lock_guard<std::mutex> lock(_db_mutex);
        // 如果会话不存在，则插入新会话
        // 构建插入SQL语句
        std::string insertSQL = "INSERT INTO Sessions (sessionId, modelName, createdAt, updatedAt) VALUES (?, ?, ?, ?);";

        // 准备SQL语句
        sqlite3_stmt *stmt;
        int rc = sqlite3_prepare_v2(_db, insertSQL.c_str(), -1, &stmt, nullptr);
        if (rc != SQLITE_OK)
        {
            ERRO("Failed to prepare SQL statement: {}", sqlite3_errmsg(_db));
            return false;
        }

        // 绑定参数
        sqlite3_bind_text(stmt, 1, session._sessionId.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 2, session._modelName.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_int64(stmt, 3, static_cast<sqlite3_int64>(session._createdAt));
        sqlite3_bind_int64(stmt, 4, static_cast<sqlite3_int64>(session._updatedAt));

        // 执行SQL语句
        rc = sqlite3_step(stmt);
        if (rc != SQLITE_DONE)
        {
            ERRO("Failed to insert session: {}", sqlite3_errmsg(_db));
            sqlite3_finalize(stmt);
            return false;
        }

        // 释放资源
        sqlite3_finalize(stmt);
        INFO("Session inserted successfully: {}", session._sessionId);
        return true;
    }
    // 获取指定SessionId会话
    std::shared_ptr<Session> DataManager::querySession(const std::string &sessionId) const
    {
        _db_mutex.lock(); // 加锁保护数据库操作的线程安全
         if (!_db)
        {
            ERRO("Database is not open");
            _db_mutex.unlock();
            return nullptr;
        }
        // 构建查询SQL语句
        std::string querySQL = "SELECT modelName, createdAt, updatedAt FROM Sessions WHERE sessionId = ?;";

        // 准备SQL语句
        sqlite3_stmt *stmt;
        int rc = sqlite3_prepare_v2(_db, querySQL.c_str(), -1, &stmt, nullptr);
        if (rc != SQLITE_OK)
        {
            ERRO("Failed to prepare SQL statement: {}", sqlite3_errmsg(_db));
            _db_mutex.unlock(); // 解锁
            return nullptr;
        }

        // 绑定参数
        sqlite3_bind_text(stmt, 1, sessionId.c_str(), -1, SQLITE_TRANSIENT);

        // 执行SQL语句
        rc = sqlite3_step(stmt);
        if (rc != SQLITE_ROW)
        {
            ERRO("Failed to query session: {}", sqlite3_errmsg(_db));
            sqlite3_finalize(stmt);
            _db_mutex.unlock(); // 解锁
            return nullptr;
        }

        // 获取查询结果
        std::string modelNameResult = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 0));
        int64_t createdAtResult = sqlite3_column_int64(stmt, 1);
        int64_t updatedAtResult = sqlite3_column_int64(stmt, 2);

        // 释放资源
        sqlite3_finalize(stmt);
        
        // 创建并返回Session对象
        auto session = std::make_shared<Session>(modelNameResult);
        session->_sessionId = sessionId;
        session->_createdAt = static_cast<std::time_t>(createdAtResult);
        session->_updatedAt = static_cast<std::time_t>(updatedAtResult);
        _db_mutex.unlock(); // 解锁
        session->_messages = queryMessages(sessionId); // 查询并设置会话的消息列表
        INFO("Session queried successfully: {}", session->_sessionId);
        return session;
    }

    // 更新指定Session的时间戳
    bool DataManager::updateSessionTimestamp(const std::string &sessionId, std::time_t timestamp)
    {
        std::lock_guard<std::mutex> lock(_db_mutex);
        // 构建更新SQL语句
        std::string updateSQL = "UPDATE Sessions SET updatedAt = ? WHERE sessionId = ?;";
        // 准备SQL语句
        sqlite3_stmt *stmt;
        int rc = sqlite3_prepare_v2(_db, updateSQL.c_str(), -1, &stmt, nullptr);
        if (rc != SQLITE_OK)
        {
            ERRO("Failed to prepare SQL statement: {}", sqlite3_errmsg(_db));
            return false;
        }
        // 绑定参数
        sqlite3_bind_int64(stmt, 1, static_cast<sqlite3_int64>(timestamp));
        sqlite3_bind_text(stmt, 2, sessionId.c_str(), -1, SQLITE_TRANSIENT);
        // 执行SQL语句
        rc = sqlite3_step(stmt);
        if (rc != SQLITE_DONE)
        {
            ERRO("Failed to update session timestamp: {}", sqlite3_errmsg(_db));
            sqlite3_finalize(stmt);
            return false;
        }
        // 释放资源
        sqlite3_finalize(stmt);
        INFO("Session timestamp updated successfully: {}", sessionId);
        return true;
    }

    // 删除指定Session,注意删除Session会级联删除相关的Message
    bool DataManager::deleteSession(const std::string &sessionId)
    {
        std::lock_guard<std::mutex> lock(_db_mutex);
        // 构建删除SQL语句
        std::string deleteSQL = "DELETE FROM Sessions WHERE sessionId = ?;";
        // 准备SQL语句
        sqlite3_stmt *stmt;
        int rc = sqlite3_prepare_v2(_db, deleteSQL.c_str(), -1, &stmt, nullptr);
        if (rc != SQLITE_OK)
        {
            ERRO("Failed to prepare SQL statement: {}", sqlite3_errmsg(_db));
            return false;
        }
        // 绑定参数
        sqlite3_bind_text(stmt, 1, sessionId.c_str(), -1, SQLITE_TRANSIENT);
        // 执行SQL语句
        rc = sqlite3_step(stmt);
        if (rc != SQLITE_DONE)
        {
            ERRO("Failed to delete session: {}", sqlite3_errmsg(_db));
            sqlite3_finalize(stmt);
            return false;
        }
        // 释放资源
        sqlite3_finalize(stmt);
        INFO("Session deleted successfully: {}", sessionId);
        return true;
    }

    // 获取所有会话id
    std::vector<std::string> DataManager::getAllSessionIds() const
    {
        std::lock_guard<std::mutex> lock(_db_mutex);
        std::vector<std::string> sessionIds;
        // 构建查询SQL语句
        std::string querySQL = "SELECT sessionId FROM Sessions ORDER BY updatedAt DESC;";
        // 准备SQL语句
        sqlite3_stmt *stmt;
        int rc = sqlite3_prepare_v2(_db, querySQL.c_str(), -1, &stmt, nullptr);
        if (rc != SQLITE_OK)
        {
            ERRO("Failed to prepare SQL statement: {}", sqlite3_errmsg(_db));
            return sessionIds;
        }
        // 执行SQL语句并获取结果
        while ((rc = sqlite3_step(stmt)) == SQLITE_ROW)
        {
            std::string sessionIdResult = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 0));
            sessionIds.push_back(sessionIdResult);
        }
        if (rc != SQLITE_DONE)
        {
            ERRO("Failed to query session IDs: {}", sqlite3_errmsg(_db));
        }
        // 释放资源
        sqlite3_finalize(stmt);
        INFO("Queried all session IDs successfully, count: {}", sessionIds.size());
        return sessionIds;
    }

    // 获取所有会话信息
    std::vector<std::shared_ptr<Session>> DataManager::getAllSessions() const
    {
        std::lock_guard<std::mutex> lock(_db_mutex);
        std::vector<std::shared_ptr<Session>> sessions;
        // 构建查询SQL语句
        std::string querySQL = "SELECT sessionId, modelName, createdAt, updatedAt FROM Sessions ORDER BY updatedAt DESC;";
        // 准备SQL语句
        sqlite3_stmt *stmt;
        int rc = sqlite3_prepare_v2(_db, querySQL.c_str(), -1, &stmt, nullptr);
        if (rc != SQLITE_OK)
        {
            ERRO("Failed to prepare SQL statement: {}", sqlite3_errmsg(_db));
            return sessions;
        }
        // 执行SQL语句并获取结果
        while ((rc = sqlite3_step(stmt)) == SQLITE_ROW)
        {
            std::string sessionIdResult = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 0));
            std::string modelNameResult = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 1));
            int64_t createdAtResult = sqlite3_column_int64(stmt, 2);
            int64_t updatedAtResult = sqlite3_column_int64(stmt, 3);
            auto session = std::make_shared<Session>(modelNameResult);
            session->_sessionId = sessionIdResult;
            session->_createdAt = static_cast<std::time_t>(createdAtResult);
            session->_updatedAt = static_cast<std::time_t>(updatedAtResult);
            // 历史消息暂时不获取，有需要时通过会话id获取即可
            sessions.push_back(session);
        }
        if (rc != SQLITE_DONE)
        {
            ERRO("Failed to query sessions: {}", sqlite3_errmsg(_db));
        }
        // 释放资源
        sqlite3_finalize(stmt);
        INFO("Queried all sessions successfully, count: {}", sessions.size());
        return sessions;
    }

    // 获取会话总数
    size_t DataManager::getSessionCount() const
    {
        std::lock_guard<std::mutex> lock(_db_mutex);
        size_t count = 0;
        // 构建查询SQL语句
        std::string querySQL = "SELECT COUNT(*) FROM Sessions;";
        // 准备SQL语句
        sqlite3_stmt *stmt;
        int rc = sqlite3_prepare_v2(_db, querySQL.c_str(), -1, &stmt, nullptr);
        if (rc != SQLITE_OK)
        {
            ERRO("Failed to prepare SQL statement: {}", sqlite3_errmsg(_db));
            return count;
        }
        // 执行SQL语句并获取结果
        rc = sqlite3_step(stmt);
        if (rc == SQLITE_ROW)
        {
            count = sqlite3_column_int(stmt, 0);
        }
        else
        {
            ERRO("Failed to query session count: {}", sqlite3_errmsg(_db));
        }
        // 释放资源
        sqlite3_finalize(stmt);
        INFO("Queried session count successfully: {}", count);
        return count;
    }

    // 删除所有会话
    bool DataManager::deleteAllSessions()
    {
        std::lock_guard<std::mutex> lock(_db_mutex);
        // 构建删除SQL语句
        std::string deleteSQL = "DELETE FROM Sessions;";
        // 执行SQL语句
        char *errMsg = nullptr;
        int rc = sqlite3_exec(_db, deleteSQL.c_str(), nullptr, nullptr, &errMsg);
        if (rc != SQLITE_OK)
        {
            ERRO("Failed to delete all sessions: {}", errMsg);
            sqlite3_free(errMsg);
            return false;
        }
        INFO("Deleted all sessions successfully");
        return true;
    }
    /////////////////////////////////////////////////////////////////
    // 插入消息,注意，插入消息是还要更新会话的时间戳
    bool DataManager::insertMessage(const std::string &sessionId, const Message &message)
    {
        _db_mutex.lock(); // 加锁保护数据库操作的线程安全
         if (!_db)
        {
            ERRO("Database is not open");
            _db_mutex.unlock();
            return false;
        }
        // 构建插入SQL语句
        std::string insertSQL = "INSERT INTO Message (messageId, sessionId, role, content, timestamp) VALUES (?, ?, ?, ?, ?);";
        // 准备SQL语句
        sqlite3_stmt *stmt;
        int rc = sqlite3_prepare_v2(_db, insertSQL.c_str(), -1, &stmt, nullptr);
        if (rc != SQLITE_OK)
        {
            ERRO("Failed to prepare SQL statement: {}", sqlite3_errmsg(_db));
            _db_mutex.unlock(); // 解锁
            return false;
        }
        // 绑定参数
        sqlite3_bind_text(stmt, 1, message._messageid.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 2, sessionId.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 3, message._role.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 4, message._content.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_int64(stmt, 5, static_cast<sqlite3_int64>(message._timestamp));
        // 执行SQL语句
        rc = sqlite3_step(stmt);
        if (rc != SQLITE_DONE)
        {
            ERRO("Failed to insert message: {}", sqlite3_errmsg(_db));
            sqlite3_finalize(stmt);
            _db_mutex.unlock(); // 解锁
            return false;
        }
        // 释放资源
        sqlite3_finalize(stmt);
        // 更新会话的时间戳
        _db_mutex.unlock(); // 解锁
        updateSessionTimestamp(sessionId, message._timestamp);
        INFO("Inserted message successfully, messageId: {}", message._messageid);
        return true;
    }

    // 获取会话的历史消息
    std::vector<Message> DataManager::queryMessages(const std::string &sessionId) const
    {
        std::lock_guard<std::mutex> lock(_db_mutex);
        std::vector<Message> messages;
        // 构建查询SQL语句
        std::string querySQL = "SELECT messageId, role, content, timestamp FROM Message WHERE sessionId = ? ORDER BY timestamp ASC;";
        // 准备SQL语句
        sqlite3_stmt *stmt;
        int rc = sqlite3_prepare_v2(_db, querySQL.c_str(), -1, &stmt, nullptr);
        if (rc != SQLITE_OK)
        {
            ERRO("Failed to prepare SQL statement: {}", sqlite3_errmsg(_db));
            return messages;
        }
        // 绑定参数
        sqlite3_bind_text(stmt, 1, sessionId.c_str(), -1, SQLITE_TRANSIENT);
        // 执行SQL语句并获取结果
        while ((rc = sqlite3_step(stmt)) == SQLITE_ROW)
        {
            std::string messageIdResult = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 0));
            std::string roleResult = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 1));
            std::string contentResult = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 2));
            int64_t timestampResult = sqlite3_column_int64(stmt, 3);
            Message message(roleResult, contentResult);
            message._messageid = messageIdResult;
            message._timestamp = static_cast<std::time_t>(timestampResult);
            messages.push_back(message);
        }
        if (rc != SQLITE_DONE)
        {
            ERRO("Failed to query messages: {}", sqlite3_errmsg(_db));
        }
        // 释放资源
        sqlite3_finalize(stmt);
        INFO("Queried messages successfully for sessionId: {}, count: {}", sessionId, messages.size());
        return messages;
    }

    // 删除指定会话的所有消息
    bool DataManager::deleteMessages(const std::string &sessionId)
    {
        std::lock_guard<std::mutex> lock(_db_mutex);
        // 构建删除SQL语句
        std::string deleteSQL = "DELETE FROM Message WHERE sessionId = ?;";
        // 准备SQL语句
        sqlite3_stmt *stmt;
        int rc = sqlite3_prepare_v2(_db, deleteSQL.c_str(), -1, &stmt, nullptr);
        if (rc != SQLITE_OK)
        {
            ERRO("Failed to prepare SQL statement: {}", sqlite3_errmsg(_db));
            return false;
        }
        // 绑定参数
        sqlite3_bind_text(stmt, 1, sessionId.c_str(), -1, SQLITE_TRANSIENT);
        // 执行SQL语句
        rc = sqlite3_step(stmt);
        if (rc != SQLITE_DONE)
        {
            ERRO("Failed to delete messages: {}", sqlite3_errmsg(_db));
            sqlite3_finalize(stmt);
            return false;
        }
        // 释放资源
        sqlite3_finalize(stmt);
        INFO("Deleted messages successfully for sessionId: {}", sessionId);
        return true;
    }

} // namespace ai_chat_sdk
