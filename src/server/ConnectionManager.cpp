#include "server/ConnectionManager.hpp"
#include <unistd.h>
#include <iostream>

ConnectionManager::ConnectionManager()
{
    std::cout << "[ConnectionManager] created.\n";
}

ConnectionManager::~ConnectionManager()
{
    std::unique_lock<std::shared_mutex> lock(mutex_);
    std::cout << "[ConnectionManager] destroyed, active connections=" << connections_.size() << "\n";
    connections_.clear(); // 自动销毁所有连接
}

void ConnectionManager::add(int fd, struct sockaddr_in &client_addr)
{
    std::unique_lock<std::shared_mutex> lock(mutex_);
    connections_[fd] = std::make_shared<Connection>(fd, client_addr);
}

// 修改 remove 方法，返回被移除的 shared_ptr
std::shared_ptr<Connection> ConnectionManager::remove(int fd)
{
    std::unique_lock<std::shared_mutex> lock(mutex_);
    auto it = connections_.find(fd);
    if (it != connections_.end())
    {
        auto conn = it->second;
        connections_.erase(it);
        return conn; // 返回 shared_ptr，延长生命周期用于清理
    }
    return nullptr;
}

// 添加安全获取方法，在获取时检查连接是否即将被移除
std::shared_ptr<Connection> ConnectionManager::get(int fd)
{
    std::shared_lock<std::shared_mutex> lock(mutex_);
    auto it = connections_.find(fd);
    if (it != connections_.end() && it->second->isValid())
    {
        return it->second;
    }
    return nullptr;
}

std::vector<std::shared_ptr<Connection>> ConnectionManager::clear_timeout_connections(int timeout_sec)
{
    std::vector<std::shared_ptr<Connection>> close_conns;
    auto now = std::chrono::steady_clock::now();
    std::unique_lock<std::shared_mutex> lock(mutex_);

    for (auto it = connections_.begin(); it != connections_.end();)
    {
        if (it->second && it->second->isTimeout(now, timeout_sec))
        {
            close_conns.push_back(it->second); // 先保存 shared_ptr
            it = connections_.erase(it);       // 再删除 map
        }
        else
            ++it;
    }
    return close_conns;
}

size_t ConnectionManager::size()
{
    std::shared_lock<std::shared_mutex> lock(mutex_);
    return connections_.size();
}

void ConnectionManager::clear()
{
    std::unique_lock<std::shared_mutex> lock(mutex_);
    connections_.clear(); // 自动销毁所有连接
}