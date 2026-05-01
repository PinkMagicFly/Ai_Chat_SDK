#include "ChatServer.h"

#include <gflags/gflags.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cctype>
#include <csignal>
#include <cstdlib>
#include <chrono>
#include <iostream>
#include <sstream>
#include <string>
#include <thread>
#include <unistd.h>

// gflags 会在下面通过 DEFINE_* 声明这些参数，这里先声明，方便在命名空间里读写。
DECLARE_string(host);
DECLARE_int32(port);
DECLARE_string(log_level);
DECLARE_string(config_file);
DECLARE_double(temperature);
DECLARE_int32(max_tokens);
DECLARE_string(ollama_model_name);
DECLARE_string(ollama_model_desc);
DECLARE_string(ollama_endpoint);

namespace ai_chat_server
{
    // 应用版本号和帮助输出中使用的固定名字。
    constexpr const char *kVersion = "1.0.0";
    constexpr const char *kAppName = "AIChatServer";
    constexpr const char *kConfigFileName = "config.conf";

    // 用于接收 Ctrl+C / SIGTERM 的退出信号，主线程据此结束阻塞等待。
    std::atomic<bool> g_shutdownRequested{false};

    // 去除字符串首尾空白，方便处理环境变量和日志级别。
    std::string trim(const std::string &value)
    {
        const auto begin = value.find_first_not_of(" \t\r\n");
        if (begin == std::string::npos)
        {
            return "";
        }
        const auto end = value.find_last_not_of(" \t\r\n");
        return value.substr(begin, end - begin + 1);
    }

    // 全部转成小写，用于日志级别、配置 key 的统一比较。
    std::string toLower(std::string value)
    {
        std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch)
                       { return static_cast<char>(std::tolower(ch)); });
        return value;
    }

    // 从 argv 里先找 config_file，只用于决定应该把哪个文件交给 gflags 去读取。
    std::string extractConfigFilePath(int argc, char *argv[])
    {
        for (int i = 1; i < argc; ++i)
        {
            const std::string arg = argv[i] == nullptr ? "" : argv[i];
            const std::string prefix = "--config_file=";
            if (arg.rfind(prefix, 0) == 0)
            {
                return arg.substr(prefix.size());
            }
            if (arg == "--config_file" && i + 1 < argc)
            {
                return argv[i + 1] == nullptr ? "" : std::string(argv[i + 1]);
            }
        }
        return "";
    }

    // 取当前可执行文件所在目录，默认配置文件就放在这个目录里。
    std::string getExecutableDir()
    {
#ifdef __linux__
        std::array<char, 4096> buffer{};
        const ssize_t len = ::readlink("/proc/self/exe", buffer.data(), buffer.size() - 1);
        if (len > 0)
        {
            std::string exePath(buffer.data(), static_cast<size_t>(len));
            const auto slash = exePath.find_last_of('/');
            if (slash != std::string::npos)
            {
                return exePath.substr(0, slash);
            }
        }
#endif
        return ".";
    }

    // 如果用户没有显式传 config_file，就默认读可执行文件目录下的 config.conf。
    std::string resolveConfigPath(const std::string &configFileFlag)
    {
        if (!configFileFlag.empty())
        {
            return configFileFlag;
        }

        const std::string exeDir = getExecutableDir();
        if (exeDir.empty() || exeDir == ".")
        {
            return kConfigFileName;
        }
        return exeDir + "/" + kConfigFileName;
    }

    // 从环境变量中读取 API Key。
    std::string getEnvValue(const char *name)
    {
        const char *value = std::getenv(name);
        if (value == nullptr)
        {
            return "";
        }
        return trim(value);
    }

    // 把字符串日志级别转换成 spdlog 的枚举值。
    spdlog::level::level_enum parseLogLevel(const std::string &logLevel)
    {
        const std::string normalized = toLower(trim(logLevel));
        if (normalized == "trace")
        {
            return spdlog::level::trace;
        }
        if (normalized == "debug")
        {
            return spdlog::level::debug;
        }
        if (normalized == "info")
        {
            return spdlog::level::info;
        }
        if (normalized == "warn" || normalized == "warning")
        {
            return spdlog::level::warn;
        }
        if (normalized == "error")
        {
            return spdlog::level::err;
        }
        if (normalized == "critical" || normalized == "crit")
        {
            return spdlog::level::critical;
        }
        if (normalized == "off")
        {
            return spdlog::level::off;
        }
        return spdlog::level::info;
    }

    // 提前处理 -v / --version，避免走完整启动流程。
    bool isVersionFlagPresent(int argc, char *argv[])
    {
        for (int i = 1; i < argc; ++i)
        {
            const std::string arg = argv[i] == nullptr ? "" : argv[i];
            if (arg == "-v" || arg == "--version")
            {
                return true;
            }
        }
        return false;
    }

    // 提前处理 -h / --help，输出我们自己整理过的帮助说明。
    bool isHelpFlagPresent(int argc, char *argv[])
    {
        for (int i = 1; i < argc; ++i)
        {
            const std::string arg = argv[i] == nullptr ? "" : argv[i];
            if (arg == "-h" || arg == "--help" || arg == "-help")
            {
                return true;
            }
        }
        return false;
    }

    // 帮助说明里包含启动方式、参数说明、示例和接口说明。
    std::string buildHelpText()
    {
        std::ostringstream oss;
        oss << "AIChatServer 使用说明\n\n";
        oss << "启动方式:\n";
        oss << "  ./AIChatServer\n";
        oss << "  ./AIChatServer --help\n";
        oss << "  ./AIChatServer --version\n\n";
        oss << "配置来源优先级:\n";
        oss << "  1. 命令行参数\n";
        oss << "  2. --config_file 指定的配置文件\n";
        oss << "  3. 可执行文件目录下的 config.conf\n";
        oss << "  4. 默认值 / 环境变量\n\n";
        oss << "配置文件格式:\n";
        oss << "  使用 gflags flagfile 格式，每行一个 --name=value\n";
        oss << "  例如：--host=0.0.0.0\n\n";
        oss << "环境变量:\n";
        oss << "  deepseek_apikey        DeepSeek 接口密钥\n";
        oss << "  openrouter_apikey      OpenRouter 接口密钥\n\n";
        oss << "参数说明:\n";
        oss << "  --host                 监听地址，默认 0.0.0.0\n";
        oss << "  --port                 监听端口，默认 8807\n";
        oss << "  --log_level            日志级别，默认 INFO\n";
        oss << "  --config_file          配置文件路径，默认可执行文件目录下的 config.conf\n";
        oss << "  --temperature          温度值，默认 0.7，范围 0~2\n";
        oss << "  --max_tokens           最大 token 数，默认 2048，不能为负数\n";
        oss << "  --ollama_model_name    Ollama 模型名称，不能为空\n";
        oss << "  --ollama_model_desc    Ollama 模型描述，不能为空\n";
        oss << "  --ollama_endpoint      Ollama 服务地址，不能为空\n";
        oss << "  --version              显示版本号\n\n";
        oss << "使用案例:\n";
        oss << "  ./AIChatServer --ollama_model_name=gemma3:270m --ollama_model_desc=\"A tiny model\" --ollama_endpoint=http://localhost:11434\n";
        oss << "  ./AIChatServer --host=127.0.0.1 --port=8807 --temperature=0.9 --max_tokens=4096\n";
        oss << "  ./AIChatServer --log_level=debug\n\n";
        oss << "ChatServer 接口说明:\n";
        oss << "  POST   /api/session             创建会话，body: {\"model\":\"模型名\"}\n";
        oss << "  GET    /api/sessions            获取会话列表\n";
        oss << "  GET    /api/models              获取模型列表\n";
        oss << "  DELETE /api/session/{id}        删除会话\n";
        oss << "  GET    /api/session/{id}/history 获取会话历史消息\n";
        oss << "  POST   /api/message             全量返回消息，body: {\"session_id\":\"...\",\"message\":\"...\"}\n";
        oss << "  POST   /api/message/async       流式返回消息，body: {\"session_id\":\"...\",\"message\":\"...\"}\n";
        return oss.str();
    }

    void printVersion()
    {
        std::cout << kAppName << " version " << kVersion << std::endl;
    }

    // 对启动参数做安全检查，防止把非法配置传给 ChatServer。
    bool validateConfig(const ai_chat_server::ServerConfig &config, std::string &error)
    {
        if (config.host.empty())
        {
            error = "host 不能为空";
            return false;
        }
        if (config.port <= 0 || config.port > 65535)
        {
            error = "port 必须在 1~65535 之间";
            return false;
        }
        if (config.temperature < 0.0 || config.temperature > 2.0)
        {
            error = "temperature 必须在 0~2 之间";
            return false;
        }
        if (config.maxTokens < 0)
        {
            error = "maxTokens 不能为负数";
            return false;
        }
        if (config.deepseekAPIKEY.empty() && config.openrouterAPIKEY.empty())
        {
            error = "至少需要提供一个云端模型 API Key，环境变量 deepseek_apikey 或 openrouter_apikey 不能为空";
            return false;
        }
        if (config.ollamaModelName.empty() || config.ollamaEndpoint.empty() || config.ollamaModelDesc.empty())
        {
            error = "ollama 配置不能为空，ollamaModelName、ollamaModelDesc、ollamaEndpoint 都必须提供";
            return false;
        }

        const std::string normalizedLevel = toLower(config.logLevel);
        static const std::unordered_map<std::string, bool> kAllowedLevels = {
            {"trace", true}, {"debug", true}, {"info", true}, {"warn", true}, {"warning", true}, {"error", true}, {"critical", true}, {"crit", true}, {"off", true}};
        if (kAllowedLevels.find(normalizedLevel) == kAllowedLevels.end())
        {
            error = "logLevel 必须是 TRACE、DEBUG、INFO、WARN、ERROR、CRITICAL 或 OFF";
            return false;
        }

        return true;
    }

    // SIGINT / SIGTERM 到来时只设置退出标记，不在信号处理函数里做复杂逻辑。
    void requestShutdown(int)
    {
        g_shutdownRequested.store(true);
    }

} // namespace

// gflags 参数定义。默认值要和需求保持一致，方便直接运行。
DEFINE_string(host, "0.0.0.0", "监听地址");
DEFINE_int32(port, 8807, "监听端口");
DEFINE_string(log_level, "INFO", "日志级别");
DEFINE_string(config_file, "", "配置文件路径，默认当前目录下的 config.conf");
DEFINE_double(temperature, 0.7, "生成随机程度，范围 0~2");
DEFINE_int32(max_tokens, 2048, "最大 token 数");
DEFINE_string(ollama_model_name, "gemma3:270m", "Ollama 模型名称");
DEFINE_string(ollama_model_desc, "Gemma 3是一款由Ollama提供的高性能小型语言模型，具有出色的自然语言理解和生成能力，适用于各种对话场景，提供流畅自然的交互体验。", "Ollama 模型描述");
DEFINE_string(ollama_endpoint, "http://localhost:11434", "Ollama 服务地址");

int main(int argc, char *argv[])
{
    // 把自定义帮助文本交给 gflags，兼容其默认的 usage 输出。
    google::SetUsageMessage(ai_chat_server::buildHelpText());

    // 用户手动输入 -h/--help 时，直接打印帮助并退出。
    if (ai_chat_server::isHelpFlagPresent(argc, argv))
    {
        std::cout << ai_chat_server::buildHelpText();
        return 0;
    }

    // 用户手动输入 -v/--version 时，直接打印版本并退出。
    if (ai_chat_server::isVersionFlagPresent(argc, argv))
    {
        ai_chat_server::printVersion();
        return 0;
    }

    // 先从 argv 里拿到配置文件路径，再交给 gflags 自己读取 flagfile。
    const std::string configFilePath = ai_chat_server::resolveConfigPath(ai_chat_server::extractConfigFilePath(argc, argv));
    if (!google::ReadFromFlagsFile(configFilePath, argv[0], true))
    {
        std::cerr << "[ERROR] Failed to read gflags config file: " << configFilePath << std::endl;
        return 1;
    }

    // 再解析命令行参数，命令行优先级高于配置文件。
    google::ParseCommandLineFlags(&argc, &argv, true);

    // 把最终参数组装成 ChatServer 需要的 ServerConfig。
    ai_chat_server::ServerConfig config;
    config.host = FLAGS_host;
    config.port = FLAGS_port;
    config.logLevel = FLAGS_log_level;
    config.temperature = FLAGS_temperature;
    config.maxTokens = FLAGS_max_tokens;
    config.deepseekAPIKEY = ai_chat_server::getEnvValue("deepseek_apikey");
    config.openrouterAPIKEY = ai_chat_server::getEnvValue("openrouter_apikey");
    config.ollamaModelName = FLAGS_ollama_model_name;
    config.ollamaModelDesc = FLAGS_ollama_model_desc;
    config.ollamaEndpoint = FLAGS_ollama_endpoint;

    // 启动前做一轮校验，避免空值、越界值或缺失配置导致服务异常。
    std::string configError;
    if (!ai_chat_server::validateConfig(config, configError))
    {
        std::cerr << "[ERROR] " << configError << std::endl;
        std::cerr << std::endl
                  << ai_chat_server::buildHelpText();
        return 1;
    }

    // 初始化日志系统，之后的 INFO/ERRO 宏才能正常输出。
    ct_log::logger::initLogger(ai_chat_server::kAppName, "ChatServer.log", ai_chat_server::parseLogLevel(config.logLevel));
    INFO("Starting {} version {}", ai_chat_server::kAppName, ai_chat_server::kVersion);
    INFO("Loaded config: host={}, port={}, logLevel={}, temperature={}, maxTokens={}, ollamaModelName={}, ollamaEndpoint={}",
         config.host, config.port, config.logLevel, config.temperature, config.maxTokens, config.ollamaModelName, config.ollamaEndpoint);

    // 创建并启动 HTTP 服务。
    ai_chat_server::ChatServer server(config);
    if (!server.start())
    {
        ERRO("Failed to start {}", ai_chat_server::kAppName);
        return 1;
    }

    // 启动后主线程保持阻塞，直到收到退出信号。
    INFO("{} started successfully, press Ctrl+C to stop", ai_chat_server::kAppName);

    std::signal(SIGINT, ai_chat_server::requestShutdown);
    std::signal(SIGTERM, ai_chat_server::requestShutdown);

    // 主线程轮询等待退出标记，避免程序直接结束。
    while (!ai_chat_server::g_shutdownRequested.load())
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }

    // 收到退出信号后，主动停止服务并记录退出日志。
    server.stop();
    INFO("{} exited", ai_chat_server::kAppName);
    return 0;
}
