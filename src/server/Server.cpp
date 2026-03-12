#include "server/Server.hpp"

std::mutex log_mutex;
Server::Server(int port, int thread_num) : port_(port), server_socket(), epoll_(), thread_pool_(thread_num), conn_manager_(), is_runing_(false)
{
    // 1. 先创建 epoll 实例（必须先做）
    if (!epoll_.create())
    {
        std::cerr << "Server init failed: epoll create failed" << std::endl;
        exit(EXIT_FAILURE);
    }

    // 2. 初始化 server socket
    if (!server_socket.create())
    {
        perror("socket create failed");
        exit(EXIT_FAILURE);
    }

    if (!server_socket.bind(port_))
    {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }

    if (!server_socket.listen())
    {
        perror("listen failed");
        exit(EXIT_FAILURE);
    }

    // 把监听 socket 设置为非阻塞
    if (!server_socket.setNonBlocking())
    {
        perror("setNonBlocking listen socket failed");
        exit(EXIT_FAILURE);
    }

    // 3. 给 server socket 加 epoll 事件（ET 模式）
    if (!epoll_.addFd(server_socket.getFd(), EPOLLIN | EPOLLET))
    {
        perror("epoll add server socket failed");
        exit(EXIT_FAILURE);
    }

    std::cout << "Server constructed, listening on port " << port_ << std::endl;
}

Server::~Server()
{
    stop();
}

void Server::start()
{
    is_runing_ = true;
    int timeout_sec = 60;
    std::cout << "epfd = " << epoll_.getFd() << std::endl;
    std::cout << "Server is running on port " << port_ << std::endl;

    // 主循环
    while (is_runing_)
    {
        // 等待事件，3000ms 超时以便周期性做维护（如超时清理）
        std::vector<epoll_event> events = epoll_.wait(1024, 3000);

        for (auto &ev : events)
        {
            if (ev.data.fd == server_socket.getFd())
            {
                // 监听 socket 有新连接（ET 模式下需要循环 accept）
                handleNewConnection();
            }
            else
            {
                std::shared_ptr<Connection> conn = conn_manager_.get(ev.data.fd);
                if (!conn)
                {
                    // 连接不存在（可能刚被移除），确保 epoll 清理
                    epoll_.delFd(ev.data.fd);
                    continue;
                }

                // 优先处理读事件
                if (ev.events & EPOLLIN)
                {
                    handleReadEvent(conn);
                }
                else if (ev.events & EPOLLOUT)
                {
                    handleWriteEvent(conn);
                }
                else
                {
                    // 其他事件（错误/挂断）
                    if (ev.events & (EPOLLERR | EPOLLHUP))
                    {
                        std::cout << "[FD=" << ev.data.fd << "] epoll ERR/HUP, removing" << std::endl;
                        epoll_.delFd(ev.data.fd);
                        conn_manager_.remove(ev.data.fd);
                    }
                }
            }
        }
        std::vector<std::shared_ptr<Connection>> close_conns = conn_manager_.clear_timeout_connections(timeout_sec);
        for (auto &conn : close_conns)
        {
            if (conn->getState() != ConnectionState::PROCESSING)
            {
                int fd = conn->getFd();
                epoll_.delFd(fd);
                conn->close();
                // 注意：conn_manager_.remove(fd) 已经在 clear_timeout_connections 中调用
                std::cout << "[FD=" << fd << "] connection timeout, removed" << std::endl;
            }
            else
            {
                std::cout << "[FD=" << conn->getFd() << "] skip timeout cleanup (processing)" << std::endl;
            }
        }
    }
}

void Server::stop()
{
    if (!is_runing_)
        return;
    is_runing_ = false;

    // 删除监听 fd
    epoll_.delFd(server_socket.getFd());
    server_socket.close();
    std::cout << "Server stopped" << std::endl;
}

void Server::handleNewConnection()
{
    {
        for (;;)
        {
            struct sockaddr_in client_addr;
            int client_fd = server_socket.accept(&client_addr);
            if (client_fd < 0)
            {
                if (errno == EAGAIN || errno == EWOULDBLOCK)
                {
                    // 没有更多连接
                    break;
                }
                else
                {
                    perror("accept");
                    break;
                }
            }

            // 打印客户端信息
            char ip_str[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &(client_addr.sin_addr), ip_str, INET_ADDRSTRLEN);
            std::cout << "New connection from " << ip_str << ":" << ntohs(client_addr.sin_port)
                      << ", fd=" << client_fd << std::endl;

            // 设置 client_fd 非阻塞（非常重要）
            if (!server_socket.setNonBlocking(client_fd))
            {
                std::cerr << "Warning: setNonBlocking(client_fd) failed for fd=" << client_fd << std::endl;
                ::close(client_fd);
                continue;
            }

            // 将新 fd 加入 epoll（ET + ONESHOT）
            if (!epoll_.addFd(client_fd, EPOLLIN | EPOLLET | EPOLLONESHOT))
            {
                std::cerr << "epoll add client fd failed: " << client_fd << std::endl;
                ::close(client_fd);
                continue;
            }

            // 把连接加入管理器
            conn_manager_.add(client_fd, client_addr);
        }
    }
}

// // 读取数据（ET + ONESHOT），并在请求完整时提交线程池处理
// void Server::handleReadEvent(std::shared_ptr<Connection> conn)
// {
//     int fd = conn->getFd();
//     char buf[4096];
//     bool hasData = false;

//     std::cout << "[FD=" << fd << "] handleReadEvent, state="
//               << static_cast<int>(conn->getState())
//               << ", recv_buf_size=" << conn->recv_buf_copy().size() << std::endl;
//     // 读取所有可用数据（ET 要读尽）
//     while (true)
//     {
//         ssize_t n = ::read(fd, buf, sizeof(buf));
//         if (n > 0)
//         {
//             hasData = true;
//             conn->updateActiveTime();
//             bool req_ready = conn->request()->appendToBuffer(buf, static_cast<size_t>(n));
//             if (req_ready)
//             {
//                 conn->setState(ConnectionState::PROCESSING);
//                 {
//                     std::lock_guard<std::mutex> lg(log_mutex);
//                     std::cout << "[FD=" << fd << "] Request ready, submit to thread pool, size="
//                               << conn->recv_buf_copy().size() << std::endl;
//                 }
//                 // 提交给线程池处理
//                 // 注意：这里我们传入 raw pointer（与你现有 conn_manger_ API 保持一致）。
//                 // 如果 conn_manger_ 可能在主线程中移除该 conn（例如超时），就存在生命周期风险。
//                 // 更稳妥的做法是 conn_manger_ 返回 std::shared_ptr<Connection> 并在此捕获 shared_ptr。
//                 auto conn_shared = conn_manager_.get(fd);
//                 if (conn_shared)
//                 {
//                     // if (!epoll_.modFd(fd, EPOLLIN | EPOLLET | EPOLLONESHOT))
//                     // {
//                     //     std::cout << "[FD=" << fd << "] modFd failed before processing" << std::endl;
//                     //     return;
//                     // }
//                     thread_pool_.enqueue([this, conn_shared]()
//                                          { this->processRequest(conn_shared); });
//                 }
//                 return;
//             }
//         }
//         else if (n == 0)
//         {
//             // client closed
//             epoll_.delFd(fd);
//             conn->close();
//             conn_manager_.remove(fd);
//             return;
//         }
//         else
//         {
//             if (errno == EAGAIN || errno == EWOULDBLOCK)
//             {
//                 break;
//             }
//             else
//             {
//                 perror("read");
//                 epoll_.delFd(fd);
//                 conn->close();
//                 conn_manager_.remove(fd);
//                 return;
//             }
//         }
//     }
//     if (!hasData)
//         return;
//     // 没收完，继续等待（ONE-SHOT 必须重新 arm）
//     epoll_.modFd(fd, EPOLLIN | EPOLLET | EPOLLONESHOT);
// }
void Server::handleReadEvent(std::shared_ptr<Connection> conn)
{
    int fd = conn->getFd();

    // ✅ 双重检查连接有效性
    auto current_conn = conn_manager_.get(fd);
    if (!current_conn || current_conn != conn || !conn->isValid())
    {
        std::cout << "[FD=" << fd << "] Connection invalid in handleReadEvent" << std::endl;
        return;
    }

    // ✅ 严格的状态验证
    if (conn->getState() != ConnectionState::READING)
    {
        std::cout << "[FD=" << fd << "] Invalid state: " << static_cast<int>(conn->getState())
                  << ", expected READING(1)" << std::endl;
        // 尝试恢复状态
        if (conn->getState() == ConnectionState::NEW)
        {
            conn->setState(ConnectionState::READING);
            std::cout << "[FD=" << fd << "] Recovered state from NEW to READING" << std::endl;
        }
        else
        {
            return;
        }
    }

    char buf[65536];
    bool request_complete = false;
    size_t total_read = conn->request()->getTotalReceived();
    // ✅ 添加读取超时保护
    auto read_start = std::chrono::steady_clock::now();

    while (true)
    {
        // 防止无限循环
        auto now = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(now - read_start);
        if (duration.count() > 5000)
        { // 5秒超时
            std::cout << "[FD=" << fd << "] Read operation timeout" << std::endl;
            break;
        }

        ssize_t n = ::read(fd, buf, sizeof(buf));
        if (n > 0)
        {
            conn->updateActiveTime();

            bool chunk_complete = conn->request()->appendToBuffer(buf, static_cast<size_t>(n));
            request_complete = request_complete || chunk_complete;
            conn->request()->addReceivedSize(static_cast<size_t>(n));
            // 【关键修复】添加 100 Continue 响应
            // 修改这部分代码
            auto &req = *conn->request();
            std::string expect_header = req.getHeaders("expect");
            std::cout << "[FD=" << fd << "] Expect header: '" << expect_header << "'" << std::endl; // 添加调试
            if (!expect_header.empty() && !conn->is100ContinueSent())
            {
                // 转换为小写进行比较
                std::string lower_expect = expect_header;
                std::transform(lower_expect.begin(), lower_expect.end(), lower_expect.begin(), ::tolower);
                std::cout << "[FD=" << fd << "] Lower expect: '" << lower_expect << "'" << std::endl;

                if (lower_expect.find("100-continue") != std::string::npos)
                {
                    std::string response = "HTTP/1.1 100 Continue\r\n\r\n";
                    ssize_t sent = ::send(fd, response.c_str(), response.length(), MSG_NOSIGNAL);
                    conn->set100ContinueSent(true);
                    if (sent > 0)
                    {
                        std::cout << "[FD=" << fd << "] Sent 100 Continue" << std::endl;
                    }
                    else
                    {
                        std::cout << "[FD=" << fd << "] Failed to send 100 Continue: " << strerror(errno) << std::endl;
                    }
                }
            }
            std::cout << "[FD=" << fd << "] Read " << n << " bytes, total=" << total_read
                      << ", buffer_has_data=" << !conn->request()->getRecvBufffer().empty()
                      << ", complete=" << request_complete << std::endl;
            // ✅ 如果已经完整，尽早退出读取循环
            if (request_complete)
            {
                std::cout << "[FD=" << fd << "] Request complete, breaking read loop" << std::endl;
                break;
            }
        }
        else if (n == 0)
        {
            std::cout << "[FD=" << fd << "] Client closed connection" << std::endl;
            epoll_.delFd(fd);
            conn->close();
            conn_manager_.remove(fd);
            return;
        }
        else
        {
            if (errno == EAGAIN || errno == EWOULDBLOCK)
            {
                std::cout << "[FD=" << fd << "] No more data available" << std::endl;
                break;
            }
            else
            {
                std::cout << "[FD=" << fd << "] Read error: " << strerror(errno) << std::endl;
                epoll_.delFd(fd);
                conn->close();
                conn_manager_.remove(fd);
                return;
            }
        }
    }
    if (request_complete && total_read > 0)
    {
        // ✅ 再次验证连接状态
        current_conn = conn_manager_.get(fd);
        if (!current_conn || current_conn != conn)
        {
            std::cout << "[FD=" << fd << "] Connection removed during read" << std::endl;
            return;
        }

        // ✅ 设置处理状态
        conn->setState(ConnectionState::PROCESSING);
        std::cout << "[FD=" << fd << "] Setting state to PROCESSING" << std::endl;

        // ✅ 禁用 epoll 事件
        if (!epoll_.modFd(fd, 0))
        {
            std::cout << "[FD=" << fd << "] Failed to disable epoll events, closing connection" << std::endl;
            epoll_.delFd(fd);
            conn->close();
            conn_manager_.remove(fd);
            return;
        }

        // ✅ 提交到线程池
        thread_pool_.enqueue([this, conn]()
                             { this->processRequest(conn); });

        std::cout << "[FD=" << fd << "] Successfully submitted to thread pool" << std::endl;
    }
    else
    {
        std::cout << "[FD=" << fd << "] Request not complete, total_read=" << total_read
                  << ", request_complete=" << request_complete << std::endl;

        // ✅ 重新注册读事件
        if (!epoll_.modFd(fd, EPOLLIN | EPOLLET | EPOLLONESHOT))
        {
            std::cout << "[FD=" << fd << "] Failed to re-register read events" << std::endl;
            epoll_.delFd(fd);
            conn->close();
            conn_manager_.remove(fd);
        }
    }
}
// 在工作线程中处理请求（会准备好 send_buf）
void Server::processRequest(std::shared_ptr<Connection> conn)
{
    if (!conn || !conn->isValid())
        return;

    int fd = conn->getFd();
    std::cout << "[Worker " << std::this_thread::get_id()
              << "] processRequest FD=" << fd
              << ", state=" << static_cast<int>(conn->getState()) << std::endl;

    // 在处理前检查连接是否仍然在管理器中
    auto current_conn = conn_manager_.get(fd);
    if (!current_conn || current_conn != conn)
    {
        std::lock_guard<std::mutex> lg(log_mutex);
        std::cout << "[Worker " << std::this_thread::get_id()
                  << "] Connection removed before processing, FD=" << fd << std::endl;
        return;
    }
    const std::string msg = conn->recv_buf_copy();
    // 添加详细的请求分析
    std::cout << "[Worker " << std::this_thread::get_id()
              << "] Processing request, first 100 chars: "
              << msg.substr(0, std::min(msg.size(), 100ul)) << std::endl;

    bool is_http = false;
    auto &req = *conn->request();
    if (!req.getMethod().empty())
        is_http = true;
    Response resp;
    {
        std::cout << "[Worker " << std::this_thread::get_id() << "] FD=" << conn->getFd()
                  << ", method=" << req.getMethod()
                  << ", path=" << req.getPath()
                  << ", version=" << req.getVersion()
                  << ", Connection header=" << req.getHeaders("Connection")
                  << ", isKeepAlive=" << req.isKeepAlive() << std::endl;
    }

    if (is_http)
    {
        bool ok = req.isRequestComplete();
        if (!ok)
        {
            std::cout << "[Worker " << std::this_thread::get_id()
                      << "] Request parse failed" << std::endl;
            resp.setStatusCode(400);
            resp.setHeader("Content-Type", "text/plain");
            resp.setBody("Bad Request");
            conn->setKeepAlive(false);
        }
        else
        {
            // 简单路由
            std::string path = req.getPath();
            std::string method = req.getMethod();
            if (method == "GET")
            {
                if (path == "/")
                {
                    resp.setStatusCode(200);
                    resp.setHeader("Content-Type", "text/html; charset=utf-8");
                    resp.setBody("<h1>Welcome to My Server</h1>");
                }
                else if (path == "/hello")
                {
                    resp.setStatusCode(200);
                    resp.setHeader("Content-Type", "text/plain");
                    resp.setBody("Hello from ThreadPool!");
                }
            }
            else if (method == "POST")
            {
                handleUpload(req, resp);
            }
            else
            {
                resp.setStatusCode(404);
                resp.setHeader("Content-Type", "text/plain");
                resp.setBody("Page Not Found");
            }
        }
        // 设置 Connection 头和 keep-alive 标志
        if (req.isKeepAlive())
        {
            conn->set100ContinueSent(false);
            resp.setHeader("Connection", "keep-alive");
            conn->setKeepAlive(true);
        }
        else
        {
            resp.setHeader("Connection", "close");
            conn->setKeepAlive(false);
        }
    }
    else
    {
        // 非 HTTP 简单 echo
        resp.setStatusCode(200);
        resp.setHeader("Content-Type", "text/plain");
        resp.setBody("Echo: " + msg);
        conn->setKeepAlive(false);
    }

    // 将响应写入 send_buf
    std::string resp_str = resp.toString();
    conn->append_send(resp_str.data(), resp_str.size());
    conn->setState(ConnectionState::WRITING);

    current_conn = conn_manager_.get(fd);
    if (!current_conn || current_conn != conn)
    {
        std::lock_guard<std::mutex> lg(log_mutex);
        std::cout << "[Worker " << std::this_thread::get_id()
                  << "] Connection removed during processing, FD=" << fd << std::endl;
        return;
    }

    // 注册写事件，必须包含 EPOLLONESHOT
    if (!epoll_.modFd(fd, EPOLLOUT | EPOLLET | EPOLLONESHOT))
    {
        std::lock_guard<std::mutex> lg(log_mutex);
        std::cout << "[Worker " << std::this_thread::get_id()
                  << "] modFd failed, removing FD=" << fd << std::endl;
        auto removed_conn = conn_manager_.remove(fd);
        if (removed_conn)
            removed_conn->close();
    }
    else
    {
        std::cout << "[Epoll] modFd success: fd=" << fd << ", events=EPOLLOUT|EPOLLONESHOT" << std::endl;
    }
    if (!conn->isValid())
    {
        std::cout << "[Worker] fd invalid before modFd" << std::endl;
        return;
    }
}

// 写事件处理：把 send_buf 尽量写完
void Server::handleWriteEvent(std::shared_ptr<Connection> conn)
{
    int fd = conn->getFd();
    // 检查连接有效性和状态
    if (!conn->canWrite())
    {
        std::cout << "[FD=" << fd << "] Invalid state for write, state="
                  << static_cast<int>(conn->getState()) << std::endl;
        return;
    }

    auto current_conn = conn_manager_.get(fd);
    if (!current_conn || current_conn != conn)
    {
        std::cout << "[FD=" << fd << "] Connection invalid in handleWriteEvent" << std::endl;
        return;
    }
    std::string send_data = conn->send_buf_copy(); // 复制一份，以便在写过程中修改 send_buf
    if (send_data.empty())
    {
        // 没有要发送的数据：回到读取状态
        conn->setState(ConnectionState::READING);
        epoll_.modFd(fd, EPOLLIN | EPOLLET | EPOLLONESHOT);
        return;
    }

    ssize_t total_sent = 0;
    ssize_t remaining = static_cast<ssize_t>(send_data.size());
    while (remaining > 0)
    {
        ssize_t n = conn->safeSend(send_data.data() + total_sent, static_cast<size_t>(remaining));
        if (n > 0)
        {
            total_sent += n;
            remaining -= n;
            conn->updateActiveTime();
        }
        else if (n < 0)
        {
            if (errno == EAGAIN || errno == EWOULDBLOCK)
            {
                // 把已经发送的部分从 conn 的 send buffer 删除
                conn->erase_sent(static_cast<size_t>(total_sent));
                // 写缓冲区满，稍后继续写，重新注册 EPOLLOUT
                epoll_.modFd(fd, EPOLLOUT | EPOLLET | EPOLLONESHOT);
                return;
            }
            else
            {
                // 写入错误：移除连接
                perror("write");
                epoll_.delFd(fd);
                conn->close();
                conn_manager_.remove(fd);
                return;
            }
        }
        else
        {
            // write 返回 0：通常不常见，安全地重试或关闭
            epoll_.modFd(fd, EPOLLOUT | EPOLLET | EPOLLONESHOT);
            conn->erase_sent(static_cast<size_t>(total_sent));
            return;
        }
    }

    // 全部数据已写出：从 send buffer 中擦除已发送数据
    conn->erase_sent(static_cast<size_t>(total_sent));

    if (conn->getKeepAlive())
    {
        // 重置连接以复用
        conn->resetForKeepAlive();
        epoll_.modFd(fd, EPOLLIN | EPOLLET | EPOLLONESHOT);
        std::cout << "[FD=" << fd << "] Keep-alive connection reset for next request" << std::endl;
    }
    else
    {
        // 安全关闭
        auto removed_conn = conn_manager_.remove(fd);
        if (removed_conn)
        {
            epoll_.delFd(fd);
            removed_conn->close();
            std::cout << "[FD=" << fd << "] Connection closed (no keep-alive)" << std::endl;
        }
    }
}

void Server::handleUpload(Request &req, Response &resp)
{
    std::cout << "[Upload] Content-Type: " << req.getHeaders("Content-Type") << std::endl;
    std::cout << "[Upload] Content-Length: " << req.getHeaders("Content-Length") << std::endl;
    // 在 processRequest 中修改调试信息
    std::cout << "[Upload] File uploaded: " << req.getUploadedFilePath()
              << ", size: " << req.getBodyReceived() << " bytes" << std::endl;

    // 临时保存原始请求体用于调试
    std::ofstream debug_file("/tmp/upload_debug.bin", std::ios::binary);
    debug_file.write(req.getBody().data(), req.getBody().size());
    debug_file.close();

    std::cout << "[Upload] Debug data saved to /tmp/upload_debug.bin" << std::endl;
    if (req.getMethod() != "POST")
    {
        resp.setStatusCode(405);
        resp.setBody("Method Not Allowed");
        return;
    }
    if (!req.getUploadedFilePath().empty())
    {
        resp.setStatusCode(200);
        std::string response_msg = "File uploaded successfully!\n";
        response_msg += "Filename: " + req.getUploadedFilename() + "\n";
        response_msg += "Size: " + std::to_string(req.getBodyReceived()) + " bytes\n";
        response_msg += "Saved to: " + req.getUploadedFilePath();
        resp.setBody(response_msg);
        return;
    }
    resp.setStatusCode(400);
    resp.setBody("Empty Upload");
}