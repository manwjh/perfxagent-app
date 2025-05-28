#pragma once

#include <string>
#include <functional>
#include <memory>
#include <thread>
#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/beast/websocket/ssl.hpp>
#include <boost/asio/connect.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/beast/core/buffers_to_string.hpp>
#include "websocket_message.h"

namespace perfxagent {
namespace connection {

using tcp = boost::asio::ip::tcp;
namespace websocket = boost::beast::websocket;
namespace ssl = boost::asio::ssl;

using MessageHandler = std::function<void(const std::shared_ptr<BaseMessage>&)>;
using ConnectionHandler = std::function<void()>;
using ErrorHandler = std::function<void(const std::string&)>;
using BinaryMessageHandler = std::function<void(const std::vector<uint8_t>&)>;

class WebSocketClientWrapper {
public:
    WebSocketClientWrapper();
    ~WebSocketClientWrapper();

    // 连接到WebSocket服务器
    void connect(const std::string& uri, 
                const std::string& access_token,
                const std::string& device_id,
                const std::string& client_id);
    
    // 断开连接
    void disconnect();
    
    // 发送消息
    void sendMessage(const BaseMessage& message);
    
    // 发送二进制数据（音频数据）
    void sendBinary(const std::vector<uint8_t>& data);
    
    // 设置消息处理回调
    void setMessageHandler(MessageHandler handler);
    
    // 设置二进制消息处理回调
    void setBinaryMessageHandler(BinaryMessageHandler handler);
    
    // 设置连接成功回调
    void setConnectionHandler(ConnectionHandler handler);
    
    // 设置错误处理回调
    void setErrorHandler(ErrorHandler handler);

private:
    boost::asio::io_context ioc_;
    ssl::context ctx_{ssl::context::tlsv12};
    std::unique_ptr<websocket::stream<ssl::stream<tcp::socket>>> ws_;
    std::thread io_thread_;
    bool running_ = false;
    boost::beast::flat_buffer buffer_;

    MessageHandler message_handler_;
    BinaryMessageHandler binary_message_handler_;
    ConnectionHandler connection_handler_;
    ErrorHandler error_handler_;
    
    void run();
    void onRead(boost::beast::error_code ec, std::size_t bytes_transferred);
    void onWrite(boost::beast::error_code ec, std::size_t bytes_transferred);
    void onConnect(boost::beast::error_code ec);
};

} // namespace connection
} // namespace perfxagent 