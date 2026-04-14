#include<gtest/gtest.h>
#include"../sdk/include/DeepSeekProvider.h"
#include"../sdk/include/util/myLog.h"

TEST(DeepSeekProviderTest, InitializationTest) {
    ai_chat_sdk::DeepSeekProvider provider;
    std::map<std::string, std::string> config = {
        {"apiKey", std::getenv("deepseek_apikey")},
        {"endpoint", "https://api.deepseek.com"}
    };
    EXPECT_TRUE(provider.init(config));
    EXPECT_TRUE(provider.isAvailable());
    EXPECT_EQ(provider.getModelName(), "deepseek-chat");
    EXPECT_EQ(provider.getLLMDesc(), "一款实用性强、性能优越的中文大语言模型，适用于各种对话场景，提供流畅自然的交互体验。");
}   
TEST(DeepSeekProviderTest, SendMessageTest) {
    ai_chat_sdk::DeepSeekProvider provider;
    std::map<std::string, std::string> config = {
        {"apiKey", std::getenv("deepseek_apikey")}, //从环境变量获取API密钥，确保安全性
        {"endpoint", "https://api.deepseek.com"}
    };
    provider.init(config);
    std::vector<ai_chat_sdk::Message> messages = {
        {"user", "你好，DeepSeek！"}
    };
    std::map<std::string, std::string> requestParam = {
        {"temperature", "0.7"},
        {"maxTokens", "2048"}
    };
    std::string response = provider.sendMessage(messages, requestParam);
    std::cout << "Response: " << response << std::endl;
    EXPECT_FALSE(response.empty());
}

int main(int argc, char **argv) {
    //初始化spdlog日志系统
    ct_log::logger::initLogger("DeepSeekProviderTestLogger", "stdout", spdlog::level::debug);
    //初始化Google Test框架
    ::testing::InitGoogleTest(&argc, argv);
    //运行所有测试用例
    return RUN_ALL_TESTS();
}