#include"../../include/util/myLog.h"
#include<spdlog/sinks/basic_file_sink.h>
#include<spdlog/sinks/stdout_color_sinks.h>
#include<spdlog/async.h>
#include<spdlog/spdlog.h>
#include<memory>

namespace ct_log {
    std::shared_ptr<spdlog::logger> logger::_logger = nullptr;
    std::mutex logger::_mutex;
    logger::logger() {}
    void logger::initLogger(const std::string &loggerName, const std::string &logFilePath,
                            spdlog::level::level_enum logLevel) {
        if(_logger==nullptr)
        {
            std::lock_guard<std::mutex> lock(_mutex);
            if(_logger==nullptr)
            {
                //设置全局自动刷新的日志级别，当日志级别>=logLevel时，自动刷新日志到文件
                _logger->flush_on(logLevel);
                //启用异步日志，即将日志信息存入队列中，写入操作放在单独的线程中执行，避免阻塞主线程，提高性能
                spdlog::init_thread_pool(32768, 1);//设置异步日志队列大小为32768，线程数为1
                if(logFilePath=="stdout")
                {
                    //创建一个带颜色的日志器，输出到标准输出
                    _logger=spdlog::stdout_color_mt(loggerName);
                }
                else {
                    //创建一个文件输出的日志器，日志会被写入到指定的文件中
                    _logger = spdlog::basic_logger_mt<spdlog::async_logger>(loggerName, logFilePath);
                }
            }

            //格式设置
            //[时间][日志器名称][日志级别]日志内容
            _logger->set_pattern("[%H:%M:%S][%n][%-7l]%v");
            //设置日志级别，只有日志级别>=logLevel的日志才会被记录
            _logger->set_level(logLevel);
        }
    }

    std::shared_ptr<spdlog::logger> logger::getLogger() {
        if (_logger == nullptr) {
            throw std::runtime_error("Logger not initialized. Call initLogger() first.");
        }
        return _logger;
    }
}