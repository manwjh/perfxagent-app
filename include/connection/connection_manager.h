#pragma once

#include "thread_pool.h"
#include <mutex>

namespace perfxagent {
namespace connection {

class ConnectionManager {
public:
    static ConnectionManager& getInstance();

    // 获取线程池
    ThreadPool& getThreadPool();

private:
    ConnectionManager();
    ~ConnectionManager();

    // 单例模式
    ConnectionManager(const ConnectionManager&) = delete;
    ConnectionManager& operator=(const ConnectionManager&) = delete;

    std::mutex connections_mutex_;
    ThreadPool thread_pool_;
};

} // namespace connection
} // namespace perfxagent 