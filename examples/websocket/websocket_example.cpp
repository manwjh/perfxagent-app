#include <iostream>
#include <string>
#include <thread>
#include <chrono>
#include <memory>
#include "connection/connection_manager.h"
#include "connection/websocket_message.h"

using namespace std::chrono_literals;
using namespace perfxagent::connection;

class WebSocketExample {
public:
    WebSocketExample() = default;

    void run(const std::string& uri, 
             const std::string& access_token,
             const std::string& device_id,
             const std::string& device_uuid) {
        try {
            // 创建连接
            client_ = ConnectionManager::getInstance().createConnection("example", uri);
            
            // 设置消息处理回调
            client_->setMessageHandler([this](const std::shared_ptr<BaseMessage>& msg) {
                handleMessage(msg);
            });

            // 设置错误处理回调
            client_->setErrorHandler([](const std::string& error) {
                std::cerr << "Error: " << error << std::endl;
            });

            // 设置连接处理回调
            client_->setConnectionHandler([]() {
                std::cout << "Connection established" << std::endl;
            });
            
            // 连接到服务器
            client_->connect(uri, access_token, device_id, device_uuid);
            
            // 发送hello消息
            sendHelloMessage();

            // 等待一段时间后关闭连接
            std::this_thread::sleep_for(5s);
            disconnect();

        } catch (const std::exception& e) {
            std::cerr << "Error: " << e.what() << std::endl;
        }
    }

    void disconnect() {
        if (client_) {
            ConnectionManager::getInstance().removeConnection("example");
            std::cout << "Connection closed" << std::endl;
        }
    }

private:
    void sendHelloMessage() {
        auto hello_msg = std::make_shared<HelloMessage>();
        hello_msg->version = 1;
        hello_msg->transport = "websocket";
        hello_msg->audio_params = {
            "opus",     // format
            16000,      // sample_rate
            1,          // channels
            60          // frame_duration
        };

        client_->sendMessage(*hello_msg);
        std::cout << "Hello message sent" << std::endl;
    }

    void handleMessage(const std::shared_ptr<BaseMessage>& msg) {
        std::cout << "Received message type: " << static_cast<int>(msg->type) << std::endl;

        if (msg->type == MessageType::HELLO) {
            std::cout << "Received hello response" << std::endl;
        }
    }

    std::shared_ptr<WebSocketClientWrapper> client_;
};

int main() {
    WebSocketExample example;
    
    // 配置参数
    std::string uri = "wss://aiot.llmworld.net/ws";
    std::string access_token = "your_access_token";  // 替换为实际的access token
    std::string device_id = "00:11:22:33:44:55";    // 替换为实际的MAC地址
    std::string device_uuid = "550e8400-e29b-41d4-a716-446655440000";  // 替换为实际的设备UUID

    // 运行示例
    example.run(uri, access_token, device_id, device_uuid);

    return 0;
}
