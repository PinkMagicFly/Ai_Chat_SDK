#pragma once

#include <mutex>
#include <spdlog/spdlog.h>

namespace ct_log {
class logger {
public:
  static void
  initLogger(const std::string &loggerName, const std::string &logFilePath,
             spdlog::level::level_enum logLevel = spdlog::level::info);
  static std::shared_ptr<spdlog::logger> getLogger();

private:
  logger();
  logger(const logger &) = delete;
  logger &operator=(const logger &) = delete;

private:
  static std::shared_ptr<spdlog::logger> _logger;
  static std::mutex _mutex;
};
} // namespace ct_log

//fmt库的格式化字符串宏定义，使用时可以直接调用FMT("日志内容: {}", 变量)来记录格式化信息，日志中会自动包含文件名和行号，方便定位问题
//日志宏定义，使用时可以直接调用DBG("日志内容: {}", 变量)来记录调试信息，日志中会自动包含文件名和行号，方便定位问题，可以理解为spdlog的正文，就是这里的文件名+行号+format里的内容
#define TRC(format, ...) ct_log::logger::getLogger()->trace(std::string("[{:>10s}:{:<4d}]")+format, __FILE__, __LINE__, ##__VA_ARGS__)
#define DBG(format, ...) ct_log::logger::getLogger()->debug(std::string("[{:>10s}:{:<4d}]")+format, __FILE__, __LINE__, ##__VA_ARGS__)
#define INF(format, ...) ct_log::logger::getLogger()->info(std::string("[{:>10s}:{:<4d}]")+format, __FILE__, __LINE__, ##__VA_ARGS__)
#define WRN(format, ...) ct_log::logger::getLogger()->warn(std::string("[{:>10s}:{:<4d}]")+format, __FILE__, __LINE__, ##__VA_ARGS__)
#define ERR(format, ...) ct_log::logger::getLogger()->error(std::string("[{:>10s}:{:<4d}]")+format, __FILE__, __LINE__, ##__VA_ARGS__)
#define CRIT(format, ...) ct_log::logger::getLogger()->critical(std::string("[{:>10s}:{:<4d}]")+format, __FILE__, __LINE__, ##__VA_ARGS__)
