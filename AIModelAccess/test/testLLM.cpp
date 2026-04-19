#include<gtest/gtest.h>
#include"../sdk/include/DeepSeekProvider.h"
#include"../sdk/include/ChatGPTProvider.h"
#include"../sdk/include/GeminiProvider.h"
#include"../sdk/include/util/myLog.h"
#include"../sdk/include/OllamaProvider.h"

// TEST(DeepSeekProviderTest, InitializationTest) {
//     ai_chat_sdk::DeepSeekProvider provider;
//     std::map<std::string, std::string> config = {
//         {"apiKey", std::getenv("deepseek_apikey")},
//         {"endpoint", "https://api.deepseek.com"}
//     };
//     EXPECT_TRUE(provider.init(config));
//     EXPECT_TRUE(provider.isAvailable());
//     EXPECT_EQ(provider.getModelName(), "deepseek-chat");
//     EXPECT_EQ(provider.getLLMDesc(), "一款实用性强、性能优越的中文大语言模型，适用于各种对话场景，提供流畅自然的交互体验。");
// }   
// TEST(DeepSeekProviderTest, SendMessageTest) {
//     ai_chat_sdk::DeepSeekProvider provider;
//     std::map<std::string, std::string> config = {
//         {"apiKey", std::getenv("deepseek_apikey")}, //从环境变量获取API密钥，确保安全性
//         {"endpoint", "https://api.deepseek.com"}
//     };
//     provider.init(config);
//     std::vector<ai_chat_sdk::Message> messages = {
//         {"user", "你好，DeepSeek！"}
//     };
//     std::map<std::string, std::string> requestParam = {
//         {"temperature", "0.7"},
//         {"maxTokens", "2048"}
//     };
//     std::string response = provider.sendMessage(messages, requestParam);
//     std::cout << "Response: " << response << std::endl;
//     EXPECT_FALSE(response.empty());
// }
// TEST(DeepSeekProviderTest, SendMessageStreamTest) {
//     ai_chat_sdk::DeepSeekProvider provider;
//     std::map<std::string, std::string> config = {
//         {"apiKey", std::getenv("deepseek_apikey")}, //从环境变量获取API密钥，确保安全性
//         {"endpoint", "https://api.deepseek.com"}
//     };
//     provider.init(config);
//     std::vector<ai_chat_sdk::Message> messages = {
//         {"user", "你好，DeepSeek！"}
//     };
//     std::map<std::string, std::string> requestParam = {
//         {"temperature", "0.7"},
//         {"maxTokens", "2048"}
//     };
//     auto writechunk = [](const std::string& chunk, bool isLast){
//         INFO("Received chunk: {}", chunk);
//         if(isLast)
//         {
//             INFO("Received last chunk");
//         }
//     };
//     std::string response = provider.sendMessageStream(messages, requestParam, writechunk);
//     std::cout << "Response: " << response << std::endl;
//     EXPECT_FALSE(response.empty());
// }

// TEST(ChatGPTProviderTest, InitializationTest) {
//     ai_chat_sdk::ChatGPTProvider provider;
//     std::map<std::string, std::string> config = {
//         {"apiKey", std::getenv("openrouter_apikey")},
//         {"endpoint", "https://openrouter.ai"}
//     };
//     EXPECT_TRUE(provider.init(config));
//     EXPECT_TRUE(provider.isAvailable());
//     EXPECT_EQ(provider.getModelName(), "openai/gpt-4o-mini");
//     EXPECT_EQ(provider.getLLMDesc(), "OpenAI的GPT-4o-mini是一款强大的语言模型，具有出色的自然语言理解和生成能力，适用于各种对话场景，提供流畅自然的交互体验。");
// }
// TEST(ChatGPTProviderTest, SendMessageTest) {
//     ai_chat_sdk::ChatGPTProvider provider;
//     std::map<std::string, std::string> config = {
//         {"apiKey", std::getenv("openrouter_apikey")}, //从环境变量获取API密钥，确保安全性
//         {"endpoint", "https://openrouter.ai"}
//     };
//     provider.init(config);
//     std::vector<ai_chat_sdk::Message> messages = {
//         {"user", "你好，ChatGPT！"}
//     };
//     std::map<std::string, std::string> requestParam = {
//         {"temperature", "0.7"},
//         {"max_output_tokens", "2048"}
//     };
//     std::string response = provider.sendMessage(messages, requestParam);
//     std::cout << "Response: " << response << std::endl;
//     EXPECT_FALSE(response.empty());
// }

// TEST(ChatGPTProviderTest, SendMessageStreamTest) {
//     ai_chat_sdk::ChatGPTProvider provider;
//     std::map<std::string, std::string> config = {
//         {"apiKey", std::getenv("openrouter_apikey")}, //从环境变量获取API密钥，确保安全性
//         {"endpoint", "https://openrouter.ai"}
//     };
//     provider.init(config);
//     std::vector<ai_chat_sdk::Message> messages = {
//         {"user", "你好，ChatGPT！"}
//     };
//     std::map<std::string, std::string> requestParam = {
//         {"temperature", "0.7"},
//         {"max_output_tokens", "2048"}
//     };
//     auto writechunk = [](const std::string& chunk, bool isLast){
//         INFO("Received chunk: {}", chunk);
//         if(isLast)
//         {
//             INFO("Received last chunk");
//         }
//     };
//     std::string response = provider.sendMessageStream(messages, requestParam, writechunk);
//     std::cout << "Response: " << response << std::endl;
//     EXPECT_FALSE(response.empty());
// }

// TEST(GeminiProviderTest, SendMessageTest) {
//     ai_chat_sdk::GeminiProvider provider;
//     std::map<std::string, std::string> config = {
//         {"apiKey", std::getenv("openrouter_apikey")}, //从环境变量获取API密钥，确保安全性
//         {"endpoint", "https://openrouter.ai"}
//     };
//     provider.init(config);
//     std::vector<ai_chat_sdk::Message> messages = {
//         {"user", "你好，Gemini！"}
//     };
//     std::map<std::string, std::string> requestParam = {
//         {"temperature", "0.7"},
//         {"maxTokens", "2048"}
//     };
//     std::string response = provider.sendMessage(messages, requestParam);
//     std::cout << "Response: " << response << std::endl;
//     EXPECT_FALSE(response.empty());
// }

// TEST(GeminiProviderTest, SendMessageStreamTest) {
//     ai_chat_sdk::GeminiProvider provider;
//     std::map<std::string, std::string> config = {
//         {"apiKey", std::getenv("openrouter_apikey")}, //从环境变量获取API密钥，确保安全性
//         {"endpoint", "https://openrouter.ai"}
//     };
//     provider.init(config);
//     std::vector<ai_chat_sdk::Message> messages = {
//         {"user", "你可以做什么，你是哪个模型？"}
//     };
//     std::map<std::string, std::string> requestParam = {
//         {"temperature", "0.7"},
//         {"maxTokens", "2048"}
//     };
//     auto writechunk = [](const std::string& chunk, bool isLast){
//         INFO("Received chunk: {}", chunk);
//         if(isLast)
//         {
//             INFO("Received last chunk");
//         }
//     };
//     std::string response = provider.sendMessageStream(messages, requestParam, writechunk);
//     std::cout << "Response: " << response << std::endl;
//     EXPECT_FALSE(response.empty());
// }

TEST(OllamaProviderTest, SendMessageTest) {
    ai_chat_sdk::OllamaProvider provider;
    std::map<std::string, std::string> config = {
        {"endpoint", "http://localhost:11434"}, // Ollama默认的本地服务地址
        {"modelName", "gemma3:270m"}, // Ollama中已安装的模型名称，确保在Ollama中已安装该模型
        {"modelDesc", "Gemma 3是一款由Ollama提供的高性能小型语言模型，具有出色的自然语言理解和生成能力，适用于各种对话场景，提供流畅自然的交互体验。"}
    };
    provider.init(config);
    std::vector<ai_chat_sdk::Message> messages = {
        {"user", "你好，Ollama！"}
    };
    std::map<std::string, std::string> requestParam = {
        {"temperature", "0.7"},
        {"maxTokens", "2048"}
    };
    std::string response = provider.sendMessage(messages, requestParam);
    std::cout << "Response: " << response << std::endl;
    EXPECT_FALSE(response.empty());
}

TEST(OllamaProviderTest, SendMessageStreamTest) {
    ai_chat_sdk::OllamaProvider provider;
    std::map<std::string, std::string> config = {
        {"endpoint", "http://localhost:11434"}, // Ollama默认的本地服务地址
        {"modelName", "gemma3:270m"}, // Ollama中已安装的模型名称，确保在Ollama中已安装该模型
        {"modelDesc", "Gemma 3是一款由Ollama提供的高性能小型语言模型，具有出色的自然语言理解和生成能力，适用于各种对话场景，提供流畅自然的交互体验。"}
    };
    provider.init(config);
    std::vector<ai_chat_sdk::Message> messages = {
        {"user", "你好，Ollama！"}
    };
    std::map<std::string, std::string> requestParam = {
        {"temperature", "0.7"},
        {"maxTokens", "2048"}
    };
    auto writechunk = [](const std::string& chunk, bool isLast){
        INFO("Received chunk: {}", chunk);
        if(isLast)
        {
            INFO("Received last chunk");
        }
    };
    std::string response = provider.sendMessageStream(messages, requestParam, writechunk);
    std::cout << "Response: " << response << std::endl;
    EXPECT_FALSE(response.empty());
}

int main(int argc, char **argv) {
    //初始化spdlog日志系统
    ct_log::logger::initLogger("OllamaProviderTestLogger", "stdout", spdlog::level::debug);
    //初始化Google Test框架
    ::testing::InitGoogleTest(&argc, argv);
    //运行所有测试用例  
    return RUN_ALL_TESTS();
}