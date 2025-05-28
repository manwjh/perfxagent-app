#include "connection_manager.h"

namespace perfxagent {
namespace connection {

ConnectionManager& ConnectionManager::getInstance() {
    static ConnectionManager instance;
    return instance;
}

ConnectionManager::ConnectionManager() 
    : thread_pool_(std::thread::hardware_concurrency()) {
}

ConnectionManager::~ConnectionManager() {
    std::lock_guard<std::mutex> lock(connections_mutex_);
    connections_.clear();
}

std::shared_ptr<WebSocketClientWrapper> ConnectionManager::createConnection(
    const std::string& id,
    const std::string& uri) {
    
    std::lock_guard<std::mutex> lock(connections_mutex_);
    
    // 如果连接已存在，先移除
    if (connections_.find(id) != connections_.end()) {
        removeConnection(id);
    }
    
    // 创建新的连接
    auto client = std::make_shared<WebSocketClientWrapper>();
    connections_[id] = client;
    
    // 设置错误处理回调
    client->setErrorHandler([this, id](const std::string& error) {
        (void)error;
        // 如果发生错误，自动移除连接
        removeConnection(id);
    });
    
    // 连接到服务器
    client->connect(uri, "", "", "");
    
    return client;
}

std::shared_ptr<WebSocketClientWrapper> ConnectionManager::getConnection(
    const std::string& id) {
    
    std::lock_guard<std::mutex> lock(connections_mutex_);
    auto it = connections_.find(id);
    if (it != connections_.end()) {
        return it->second;
    }
    return nullptr;
}

void ConnectionManager::removeConnection(const std::string& id) {
    std::lock_guard<std::mutex> lock(connections_mutex_);
    auto it = connections_.find(id);
    if (it != connections_.end()) {
        it->second->disconnect();
        connections_.erase(it);
    }
}

ThreadPool& ConnectionManager::getThreadPool() {
    return thread_pool_;
}

} // namespace connection
} // namespace perfxagent 