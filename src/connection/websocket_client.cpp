#include "websocket_client.h"
#include <iostream>
#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/beast/websocket/ssl.hpp>
#include <boost/asio/connect.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/url.hpp>

namespace perfxagent {
namespace connection {

WebSocketClientWrapper::WebSocketClientWrapper() {
    // 初始化SSL上下文
    ctx_.set_verify_mode(ssl::verify_none);
}

WebSocketClientWrapper::~WebSocketClientWrapper() {
    disconnect();
}

void WebSocketClientWrapper::connect(const std::string& uri,
                                   const std::string& access_token,
                                   const std::string& device_id,
                                   const std::string& client_id) {
    try {
        // 解析URL
        boost::urls::url_view url(uri);
        std::string host = url.host();
        std::string port = url.port();
        std::string path = url.path();

        // 创建WebSocket流
        ws_ = std::make_unique<websocket::stream<ssl::stream<tcp::socket>>>(ioc_, ctx_);
        
        // 设置握手超时
        ws_->set_option(websocket::stream_base::timeout::suggested(
            boost::beast::role_type::client));

        // 设置自定义头部
        ws_->set_option(websocket::stream_base::decorator(
            [&](websocket::request_type& req) {
                req.set(boost::beast::http::field::authorization, 
                       "Bearer " + access_token);
                req.set("Protocol-Version", "1");
                req.set("Device-Id", device_id);
                req.set("Client-Id", client_id);
            }));

        // 解析主机名
        tcp::resolver resolver(ioc_);
        auto const results = resolver.resolve(host, port);

        // 连接
        boost::asio::connect(ws_->next_layer().next_layer(), results.begin(), results.end());
        
        // SSL握手
        ws_->next_layer().handshake(ssl::stream_base::client);

        // WebSocket握手
        ws_->handshake(host, path);

        // 启动IO线程
        running_ = true;
        io_thread_ = std::thread([this]() { run(); });

        // 开始异步读取
        ws_->async_read(
            buffer_,
            [this](boost::beast::error_code ec, std::size_t bytes_transferred) {
                onRead(ec, bytes_transferred);
            });

        if (connection_handler_) {
            connection_handler_();
        }

    } catch (const std::exception& e) {
        if (error_handler_) {
            error_handler_(e.what());
        }
    }
}

void WebSocketClientWrapper::disconnect() {
    if (ws_) {
        try {
            running_ = false;
            ws_->close(websocket::close_code::normal);
            if (io_thread_.joinable()) {
                io_thread_.join();
            }
        } catch (const std::exception& e) {
            if (error_handler_) {
                error_handler_(e.what());
            }
        }
    }
}

void WebSocketClientWrapper::sendMessage(const BaseMessage& message) {
    try {
        if (ws_) {
            auto json = serializeMessage(message);
            ws_->async_write(
                boost::asio::buffer(json.dump()),
                [this](boost::beast::error_code ec, std::size_t bytes_transferred) {
                    onWrite(ec, bytes_transferred);
                });
        }
    } catch (const std::exception& e) {
        if (error_handler_) {
            error_handler_(e.what());
        }
    }
}

void WebSocketClientWrapper::sendBinary(const std::vector<uint8_t>& data) {
    try {
        if (ws_) {
            ws_->async_write(
                boost::asio::buffer(data),
                [this](boost::beast::error_code ec, std::size_t bytes_transferred) {
                    onWrite(ec, bytes_transferred);
                });
        }
    } catch (const std::exception& e) {
        if (error_handler_) {
            error_handler_(e.what());
        }
    }
}

void WebSocketClientWrapper::setMessageHandler(MessageHandler handler) {
    message_handler_ = handler;
}

void WebSocketClientWrapper::setBinaryMessageHandler(BinaryMessageHandler handler) {
    binary_message_handler_ = handler;
}

void WebSocketClientWrapper::setConnectionHandler(ConnectionHandler handler) {
    connection_handler_ = handler;
}

void WebSocketClientWrapper::setErrorHandler(ErrorHandler handler) {
    error_handler_ = handler;
}

void WebSocketClientWrapper::run() {
    while (running_) {
        try {
            ioc_.run();
        } catch (const std::exception& e) {
            if (error_handler_) {
                error_handler_(e.what());
            }
            break;
        }
    }
}

void WebSocketClientWrapper::onRead(boost::beast::error_code ec, std::size_t /*bytes_transferred*/) {
    if (ec) {
        if (error_handler_) {
            error_handler_(ec.message());
        }
        return;
    }

    if (ws_->got_text()) {
        if (message_handler_) {
            try {
                auto message = deserializeMessage(boost::beast::buffers_to_string(buffer_.data()));
                message_handler_(message);
            } catch (const std::exception& e) {
                if (error_handler_) {
                    error_handler_(e.what());
                }
            }
        }
    } else if (ws_->got_binary()) {
        if (binary_message_handler_) {
            const auto& data = buffer_.data();
            const uint8_t* ptr = static_cast<const uint8_t*>(data.data());
            std::vector<uint8_t> binary_data(ptr, ptr + data.size());
            binary_message_handler_(binary_data);
        }
    }

    // 清空缓冲区
    buffer_.consume(buffer_.size());

    // 继续异步读取
    ws_->async_read(
        buffer_,
        [this](boost::beast::error_code ec, std::size_t bytes_transferred) {
            onRead(ec, bytes_transferred);
        });
}

void WebSocketClientWrapper::onWrite(boost::beast::error_code ec, std::size_t /*bytes_transferred*/) {
    if (ec && error_handler_) {
        error_handler_(ec.message());
    }
}

void WebSocketClientWrapper::onConnect(boost::beast::error_code ec) {
    if (ec) {
        if (error_handler_) {
            error_handler_(ec.message());
        }
        return;
    }

    if (connection_handler_) {
        connection_handler_();
    }
}

} // namespace connection
} // namespace perfxagent 