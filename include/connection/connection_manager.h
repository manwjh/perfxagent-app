#pragma once

#include "websocket_client.h"
#include "thread_pool.h"
#include <string>
#include <memory>
#include <unordered_map>
#include <mutex>

namespace perfxagent {
namespace connection {

class ConnectionManager {
public:
    static ConnectionManager& getInstance();

    // 创建新的WebSocket连接
    std::shared_ptr<WebSocketClientWrapper> createConnection(
        const std::string& id,
        const std::string& uri
    );

    // 获取已存在的连接
    std::shared_ptr<WebSocketClientWrapper> getConnection(const std::string& id);

    // 关闭并移除连接
    void removeConnection(const std::string& id);

    // 获取线程池
    ThreadPool& getThreadPool();

private:
    ConnectionManager();
    ~ConnectionManager();

    // 单例模式
    ConnectionManager(const ConnectionManager&) = delete;
    ConnectionManager& operator=(const ConnectionManager&) = delete;

    std::unordered_map<std::string, std::shared_ptr<WebSocketClientWrapper>> connections_;
    std::mutex connections_mutex_;
    ThreadPool thread_pool_;
};

} // namespace connection
} // namespace perfxagent 