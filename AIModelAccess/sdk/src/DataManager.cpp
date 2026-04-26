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
        std::lock_guard<std::mutex> lock(_db_mutex);
        // 检查会话是否已存在
        std::string checkSQL = "SELECT sessionId FROM Sessions WHERE sessionId = " + session._sessionId + ";";
        if (!executeSQL(checkSQL))
        {
            ERRO("Failed to check session existence");
            return false;
        }

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
    //获取指定SessionId会话
    std::shared_ptr<Session> DataManager::querySession(const std::string &sessionId) const
    {
        std::lock_guard<std::mutex> lock(_db_mutex);
        // 构建查询SQL语句
        std::string querySQL = "SELECT modelName, createdAt, updatedAt FROM Sessions WHERE sessionId = ?;";

        // 准备SQL语句
        sqlite3_stmt *stmt;
        int rc = sqlite3_prepare_v2(_db, querySQL.c_str(), -1, &stmt, nullptr);
        if (rc != SQLITE_OK)
        {
            ERRO("Failed to prepare SQL statement: {}", sqlite3_errmsg(_db));
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
        session->_messages = queryMessages(sessionId); // 查询并设置会话的消息列表
        INFO("Session queried successfully: {}", session->_sessionId);
        return session;
    }

    //更新指定Session的时间戳
    bool DataManager::updateSessionTimestamp(const std::string &sessionId, std::time_t timestamp)
    {
        std::lock_guard<std::mutex> lock(_db_mutex);
        // 构建更新SQL语句
        std::string updateSQL = "UPDATE Sessions SET updatedAt = ? WHERE sessionId = ?;";
        // 准备SQL语句
        sqlite3_stmt *stmt;
        int rc = sqlite3_prepare_v2(_db, updateSQL.c_str(), -1, &stmt, nullptr);
        if (rc != SQLITE_OK)        {
            ERRO("Failed to prepare SQL statement: {}", sqlite3_errmsg(_db));
            return false;
        }
        // 绑定参数
        sqlite3_bind_int64(stmt, 1, static_cast<sqlite3_int64>(timestamp));
        sqlite3_bind_text(stmt, 2, sessionId.c_str(), -1, SQLITE_TRANSIENT);
        // 执行SQL语句
        rc = sqlite3_step(stmt);
        if (rc != SQLITE_DONE)        {
            ERRO("Failed to update session timestamp: {}", sqlite3_errmsg(_db));
            sqlite3_finalize(stmt);
            return false;
        }
        // 释放资源
        sqlite3_finalize(stmt);
        INFO("Session timestamp updated successfully: {}", sessionId);
        return true;
    }

    //删除指定Session,注意删除Session会级联删除相关的Message
    bool DataManager::deleteSession(const std::string &sessionId)
    {
        std::lock_guard<std::mutex> lock(_db_mutex);
        // 构建删除SQL语句
        std::string deleteSQL = "DELETE FROM Sessions WHERE sessionId = ?;";
        // 准备SQL语句
        sqlite3_stmt *stmt;
        int rc = sqlite3_prepare_v2(_db, deleteSQL.c_str(), -1, &stmt, nullptr);
        if (rc != SQLITE_OK)        {
            ERRO("Failed to prepare SQL statement: {}", sqlite3_errmsg(_db));
            return false;
        }
        // 绑定参数
        sqlite3_bind_text(stmt, 1, sessionId.c_str(), -1, SQLITE_TRANSIENT);
        // 执行SQL语句
        rc = sqlite3_step(stmt);
        if (rc != SQLITE_DONE)        {
            ERRO("Failed to delete session: {}", sqlite3_errmsg(_db));
            sqlite3_finalize(stmt);
            return false;
        }
        // 释放资源
        sqlite3_finalize(stmt);
        INFO("Session deleted successfully: {}", sessionId);
        return true;
    }

} // namespace ai_chat_sdk
