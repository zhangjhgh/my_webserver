#include "server/Socket.hpp"
#include "server/Epoll.hpp"
#include "server/ThreadPool.hpp"
#include "server/Request.hpp"
#include "server/Response.hpp"
#include "server/Connection.hpp"
#include "server/ConnectionManager.hpp"
#include <arpa/inet.h>
#include <unistd.h>
#include <iostream>
#include <cstring>
#include <unordered_map>
#include <mutex>
#include <thread>
#include <fstream>

class Server
{
public:
    Server(int port, int thread_num = 4);
    ~Server();
    void start();
    void stop();

private:
    void handleNewConnection();                              // 处理新连接
    void handleReadEvent(std::shared_ptr<Connection> conn);  // 处理读事件
    void handleWriteEvent(std::shared_ptr<Connection> conn); // 处理写事件
    void processRequest(std::shared_ptr<Connection> conn);   // 业务处理
    void handleUpload(Request &req, Response &resp);         // 处理上传

    int port_;                       // 监听端口
    Socket server_socket;            // 监听socket
    Epoll epoll_;                    // epoll实例
    ThreadPool thread_pool_;         // 线程池
    ConnectionManager conn_manager_; // 连接管理器
    std::atomic<bool> is_runing_;    // 服务器运行标志
};