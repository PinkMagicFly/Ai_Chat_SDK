#include <sqlite3.h>
#include <string>
#include <iostream>

struct StudentInfo
{
    std::string name;
    std::string gender;
    int age;
    double gap;
    int id;
};

class StudentDB
{
public:
    StudentDB(const std::string &db_name)
    {
        // 创建并打开数据库
        if (sqlite3_open(db_name.c_str(), &_db) != SQLITE_OK)
        {
            std::cerr << "Failed to open database: " << sqlite3_errmsg(_db) << std::endl;
            sqlite3_close(_db);
            _db = nullptr;
        }
        // 初始化数据库-创建表
        if (_db && !initDataBase())
        {
            sqlite3_close(_db);
            _db = nullptr;
        }
    }

    ~StudentDB()
    {
        if (_db)
        {
            sqlite3_close(_db);
        }
    }

    // 插入学生信息
    bool insertStudent(const StudentInfo &student)
    {
        // 使用预编译语句插入数据，防止SQL注入攻击
        const std::string insert_sql = "INSERT INTO students (name, gender, age, gap) VALUES (?, ?, ?, ?);";
        sqlite3_stmt *stmt;
        if (sqlite3_prepare_v2(_db, insert_sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK)
        {
            std::cerr << "Failed to prepare statement: " << sqlite3_errmsg(_db) << std::endl;
            return false;
        }
        // 绑定参数
        sqlite3_bind_text(stmt, 1, student.name.c_str(), -1, SQLITE_TRANSIENT);   //-1表示字符串以null结尾，SQLITE_TRANSIENT表示SQLite会在需要时复制字符串
        sqlite3_bind_text(stmt, 2, student.gender.c_str(), -1, SQLITE_TRANSIENT); // stmt参数索引从1开始
        sqlite3_bind_int(stmt, 3, student.age);                                   // 绑定整数参数
        sqlite3_bind_double(stmt, 4, student.gap);                                // 绑定浮点数参数
        // 执行语句
        if (sqlite3_step(stmt) != SQLITE_DONE)
        {
            std::cerr << "Failed to execute statement: " << sqlite3_errmsg(_db) << std::endl;
            sqlite3_finalize(stmt);
            return false;
        }
        // 释放预编译语句资源
        sqlite3_finalize(stmt);
        return true;
    }

    // 查询指定学生信息
    bool queryStudents(const std::string &name = "")
    {
        // 使用预编译语句查询数据，防止SQL注入攻击
        std::string query_sql = R"(SELECT id, name, gender, age, gap FROM students where name=?;)";
        // 准备预编译语句
        sqlite3_stmt *stmt;
        if (sqlite3_prepare_v2(_db, query_sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK)
        {
            std::cerr << "Failed to prepare statement: " << sqlite3_errmsg(_db) << std::endl;
            return false;
        }
        // 绑定参数
        sqlite3_bind_text(stmt, 1, name.c_str(), -1, SQLITE_TRANSIENT);

        // 执行查询一个学生信息
        int rc= sqlite3_step(stmt);
        if (rc != SQLITE_DONE && rc != SQLITE_ROW)
        {
            std::cerr << "Failed to execute statement: " << sqlite3_errmsg(_db) << std::endl;
            sqlite3_finalize(stmt);
            return false;
        }

        // 获取查询结果
        int id = sqlite3_column_int(stmt, 0);                                              // 获取id列的整数
        std::string sname = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 1));   // 获取name列的字符串
        std::string gender = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 2)); // 获取gender列的字符串
        int age = sqlite3_column_int(stmt, 3);                                             // 获取age列的整数
        double gap = sqlite3_column_double(stmt, 4);                                       // 获取gap列的浮点数

        // 输出查询结果
        std::cout << "ID: " << id << ", Name: " << name << ", Gender: " << gender << ", Age: " << age << ", Gap: " << gap << std::endl;

        // 释放预编译语句资源
        sqlite3_finalize(stmt);
        return true;
    }

    // 查询所有学生信息
    bool queryAllStudents()
    {
        const std::string query_sql = R"(SELECT id, name, gender, age, gap FROM students;)";
        sqlite3_stmt *stmt;
        if (sqlite3_prepare_v2(_db, query_sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK)
        {
            std::cerr << "Failed to prepare statement: " << sqlite3_errmsg(_db) << std::endl;
            sqlite3_finalize(stmt);
            return false;
        }
        // 执行查询所有学生信息
        int step_result = sqlite3_step(stmt);
        if (step_result != SQLITE_ROW && step_result != SQLITE_DONE)
        {
            std::cerr << "Failed to execute statement: " << sqlite3_errmsg(_db) << std::endl;
            sqlite3_finalize(stmt);
            return false;
        }
        while (step_result == SQLITE_ROW)
        {
            int id = sqlite3_column_int(stmt, 0);                                              // 获取id列的整数
            std::string name = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 1));   // 获取name列的字符串
            std::string gender = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 2)); // 获取gender列的字符串
            int age = sqlite3_column_int(stmt, 3);                                             // 获取age列的整数
            double gap = sqlite3_column_double(stmt, 4);                                       // 获取gap列的浮点数
            // 输出查询结果
            std::cout << "ID: " << id << ", Name: " << name << ", Gender: " << gender << ", Age: " << age << ", Gap: " << gap << std::endl;

            // 获取下一行结果
            step_result = sqlite3_step(stmt);
        }

        // 检测查询结果是否正常结束
        if (step_result != SQLITE_DONE)
        {
            std::cerr << "Failed to execute statement: " << sqlite3_errmsg(_db) << std::endl;
            sqlite3_finalize(stmt);
            return false;
        }

        // 释放预编译语句资源
        sqlite3_finalize(stmt);
        return true;
    }

    // 修改学生信息
    bool updateStudent(const std::string name, const StudentInfo &student)
    {
        const std::string update_sql = R"(UPDATE students SET name=?, gender=?, age=?, gap=? WHERE name=?;)";
        sqlite3_stmt *stmt;
        if (sqlite3_prepare_v2(_db, update_sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK)
        {
            std::cerr << "Failed to prepare statement: " << sqlite3_errmsg(_db) << std::endl;
            return false;
        }
        // 绑定参数
        sqlite3_bind_text(stmt, 1, student.name.c_str(), -1, SQLITE_TRANSIENT);   //-1表示字符串以null结尾，SQLITE_TRANSIENT表示SQLite会在需要时复制字符串
        sqlite3_bind_text(stmt, 2, student.gender.c_str(), -1, SQLITE_TRANSIENT); // stmt参数索引从1开始
        sqlite3_bind_int(stmt, 3, student.age);                                   // 绑定整数参数
        sqlite3_bind_double(stmt, 4, student.gap);                                // 绑定浮点数参数
        sqlite3_bind_text(stmt, 5, name.c_str(), -1, SQLITE_TRANSIENT);           // 绑定name参数
        // 执行语句
        int step_result = sqlite3_step(stmt);
        if (step_result != SQLITE_DONE && step_result != SQLITE_ROW)
        {
            std::cerr << "Failed to execute statement: " << sqlite3_errmsg(_db) << std::endl;
            sqlite3_finalize(stmt);
            return false;
        }
        // 释放预编译语句资源
        sqlite3_finalize(stmt);
        return true;
    }

    //删除学生信息
    bool deleteStudent(const std::string &name)
    {
        const std::string delete_sql = R"(DELETE FROM students WHERE name=?;)";
        sqlite3_stmt *stmt;
        if (sqlite3_prepare_v2(_db, delete_sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK)
        {
            std::cerr << "Failed to prepare statement: " << sqlite3_errmsg(_db) << std::endl;
            return false;
        }
        // 绑定参数
        sqlite3_bind_text(stmt, 1, name.c_str(), -1, SQLITE_TRANSIENT); // 绑定name参数
        // 执行语句
        int step_result = sqlite3_step(stmt);
        if (step_result != SQLITE_DONE && step_result != SQLITE_ROW)
        {
            std::cerr << "Failed to execute statement: " << sqlite3_errmsg(_db) << std::endl;
            sqlite3_finalize(stmt);
            return false;
        }
        // 释放预编译语句资源
        sqlite3_finalize(stmt);
        return true;
    }

private:
    bool initDataBase()
    {
        const std::string create_table_sql = R"(CREATE TABLE IF NOT EXISTS students (
                                       id INTEGER PRIMARY KEY AUTOINCREMENT,
                                       name TEXT NOT NULL,
                                       gender TEXT,
                                       age INTEGER,
                                       gap REAL
                                       );)";

        if (sqlite3_exec(_db, create_table_sql.c_str(), nullptr, nullptr, nullptr) != SQLITE_OK)
        {
            std::cerr << "Failed to create table: " << sqlite3_errmsg(_db) << std::endl;
            return false;
        }
        return true;
    }

private:
    sqlite3 *_db = nullptr;
};

int main()
{
    StudentInfo student1={"张三", "男", 20, 3.5};
    StudentInfo student2={"李四", "女", 22, 3.8};
    StudentInfo student3={"王五", "男", 21, 3.2};
    StudentInfo student4={"赵六", "女", 23, 3.9};

    StudentDB db("studentDB.db");
    db.insertStudent(student1);
    db.insertStudent(student2);
    db.insertStudent(student3);
    db.insertStudent(student4);

    // 查询所有学生信息
    std::cout << "查询所有学生信息:" << std::endl;
    db.queryAllStudents();

    //修改学生信息
    student1.age = 21;
    db.updateStudent("张三", student1);
    // 查询指定学生信息
    std::cout << "查询指定学生信息:" << std::endl;
    db.queryStudents("张三");

    // 删除学生信息
    db.deleteStudent("李四");
    // 查询所有学生信息
    std::cout << "查询所有学生信息:" << std::endl;
    db.queryAllStudents();
    return 0;
}