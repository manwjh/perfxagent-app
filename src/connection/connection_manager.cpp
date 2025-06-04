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
}

ThreadPool& ConnectionManager::getThreadPool() {
    return thread_pool_;
}

} // namespace connection
} // namespace perfxagent 