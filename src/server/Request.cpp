#include "server/Request.hpp"

Request::Request() : expected_body_size(0), body_received(0), request_complete(false) {}
void Request::reset()
{
    headers.clear();
    method.clear();
    path.clear();
    version.clear();
    recv_buffer.clear();
    body.clear();
    uploaded_filename.clear();
    upload_file_path.clear();
    request_complete = false;
    expected_body_size = 0;
    body_received = 0;
    boundary.clear();
    if (ofs.is_open())
        ofs.close();
    multipart_state_ = MultipartState::START;
    file_data_start_ = 0;
    in_file_data_ = false;
}

bool Request::isKeepAlive() const
{
    std::string conn_header = getHeaders("Connection");

    // 将 Connection 头转换为小写进行比较
    std::string lower_conn;
    for (char c : conn_header)
    {
        lower_conn += std::tolower(c);
    }

    if (version == "HTTP/1.1")
    {
        // HTTP/1.1 默认保持连接，除非明确声明 "close"（任何大小写）
        return lower_conn != "close";
    }
    else if (version == "HTTP/1.0")
    {
        // HTTP/1.0 默认关闭连接，除非明确声明 "keep-alive"（任何大小写）
        return lower_conn == "keep-alive";
    }
    return false;
}

std::string Request::getHeaders(const std::string &key) const
{
    std::string lower_key = key;
    for (char &c : lower_key)
        c = std::tolower(c);
    auto it = headers.find(lower_key);
    return (it != headers.end()) ? it->second : "";
}

bool Request::appendToBuffer(const char *data, size_t len)
{
    recv_buffer.append(data, len);
    std::cout << "[Request] appendToBuffer: received " << len << " bytes, total buffer: " << recv_buffer.size() << " bytes" << std::endl;
    if (expected_body_size > 0 && body_received >= expected_body_size)
    {
        std::cout << "[Request] All expected data received, forcing completion" << std::endl;
        request_complete = true;
        return true;
    }
    // 如果头部还没解析，尝试解析
    if (headers.empty() && recv_buffer.find("\r\n\r\n") != std::string::npos)
    {
        if (parseHeadersFromBuffer())
        {
            std::cout << "[Request] Headers parsed, method: " << method << ", path: " << path << std::endl;
            if (expected_body_size == 0)
                request_complete = true;
        }
        else
            return false;
    }

    std::string contentType = getHeaders("content-type");
    if (!contentType.empty() && contentType.find("multipart/form-data") != std::string::npos)
    {
        std::cout << "[Request] Processing multipart data, current state: " << static_cast<int>(multipart_state_) << std::endl;
        parseMultipartData();
    }
    if (expected_body_size > 0 && contentType.find("multipart") == std::string::npos)
    {
        body.append(data, len);
        body_received += len;
        if (body_received >= expected_body_size)
            request_complete = true;
    }
    return request_complete;
}

bool Request::parseHeadersFromBuffer()
{
    size_t header_end = recv_buffer.find("\r\n\r\n");
    if (header_end == std::string::npos)
    {
        return false;
    }

    std::istringstream stream(recv_buffer.substr(0, header_end));
    std::string line;
    if (!std::getline(stream, line))
        return false;

    std::istringstream requestline(line);
    requestline >> method >> path >> version;

    while (std::getline(stream, line) && !line.empty())
    {
        if (line.back() == '\r')
            line.pop_back();
        size_t colon = line.find(':');
        if (colon != std::string::npos)
        {
            std::string key = line.substr(0, colon);
            std::string val = line.substr(colon + 1);
            val.erase(0, val.find_first_not_of(" "));
            val.erase(val.find_last_not_of(" ") + 1);
            headers[toLowerCopy(key)] = val;
            std::cout << "[Request] Header: " << key << ": " << val << std::endl;
        }
    }
    if (headers.count("content-length"))
        expected_body_size = std::stoul(headers["content-length"]);

    // 清理已处理部分
    clearProcessed(header_end + 4);
    return true;
}

void Request::parseMultipartData()
{
    std::cout << "[Multipart] processing, state=" << static_cast<int>(multipart_state_)
              << ", buffer size: " << recv_buffer.size() << std::endl;

    bool progress = true;
    int loop_count = 0;
    while (progress && multipart_state_ != MultipartState::END && loop_count < 10) // 防止无限循环
    {
        loop_count++;
        std::cout << "[Multipart] Loop " << loop_count << ", state: " << static_cast<int>(multipart_state_) << std::endl;

        switch (multipart_state_)
        {
        case MultipartState::START:
            std::cout << "[Multipart] Calling handleMultipartStart" << std::endl;
            progress = handleMultipartStart();
            std::cout << "[Multipart] After START, progress: " << progress << ", new state: " << static_cast<int>(multipart_state_) << std::endl;
            break;
        case MultipartState::READING_HEADERS:
            std::cout << "[Multipart] Calling handleMultipartHeaders" << std::endl;
            progress = handleMultipartHeaders();
            std::cout << "[Multipart] After HEADERS, progress: " << progress << ", new state: " << static_cast<int>(multipart_state_) << std::endl;
            break;
        case MultipartState::READING_BODY:
            std::cout << "[Multipart] Calling handleMultipartBody" << std::endl;
            progress = handleMultipartBody();
            std::cout << "[Multipart] After BODY, progress: " << progress << ", new state: " << static_cast<int>(multipart_state_) << std::endl;
            break;
        case MultipartState::END:
            std::cout << "[Multipart] Reached END state" << std::endl;
            progress = false;
            break;
        }

        std::cout << "[Multipart] Buffer size after loop " << loop_count << ": " << recv_buffer.size() << std::endl;
    }

    if (loop_count >= 10)
    {
        std::cout << "[Multipart] WARNING: Exited loop due to count limit" << std::endl;
    }
}

bool Request::handleMultipartStart()
{
    if (boundary.empty())
    {
        std::string contentType = getHeaders("content-type");
        size_t pos = contentType.find("boundary=");
        if (pos != std::string::npos)
        {
            std::string boundary_str = contentType.substr(pos + 9);

            // 清理边界字符串
            size_t end_pos = boundary.find(';');
            if (end_pos != std::string::npos)
            {
                boundary_str = boundary_str.substr(0, end_pos);
            }

            // 去除可能的引号
            if (boundary_str.size() >= 2 && boundary_str[0] == '"' && boundary_str[boundary_str.size() - 1] == '"')
            {
                boundary_str = boundary_str.substr(1, boundary_str.size() - 2);
            }
            setBoundary(boundary_str);
            std::cout << "[Multipart] Extracted boundary: '" << boundary_str << "'" << std::endl;
        }
        else
        {
            std::cout << "[Multipart] ERROR: No boundary found" << std::endl;
            return false;
        }
    }

    // 直接切换到HEADERS状态，让HEADERS状态处理边界
    multipart_state_ = MultipartState::READING_HEADERS;
    std::cout << "[Multipart] Switching to HEADERS state" << std::endl;
    return true;
}

// 处理上传文件请求头
bool Request::handleMultipartHeaders()
{
    // 修复：检查起始边界（没有\r\n前缀）
    std::string start_boundary = boundary_marker_; // "--boundary"

    if (recv_buffer.compare(0, start_boundary.size(), start_boundary) == 0)
    {
        std::cout << "[Multipart] Found START boundary at beginning of buffer" << std::endl;
        clearProcessed(start_boundary.size());

        // 跳过可能的换行
        while (!recv_buffer.empty() && (recv_buffer[0] == '\r' || recv_buffer[0] == '\n'))
        {
            recv_buffer.erase(0, 1);
        }
    }
    // 检查普通边界（有\r\n前缀）
    else
    {
        std::string normal_boundary = "\r\n" + boundary_marker_;
        if (recv_buffer.compare(0, normal_boundary.size(), normal_boundary) == 0)
        {
            std::cout << "[Multipart] Found NORMAL boundary at start of buffer" << std::endl;
            clearProcessed(normal_boundary.size());

            // 跳过可能的换行
            while (!recv_buffer.empty() && (recv_buffer[0] == '\r' || recv_buffer[0] == '\n'))
            {
                recv_buffer.erase(0, 1);
            }
        }
    }

    // 现在查找头部结束标记
    size_t end = recv_buffer.find("\r\n\r\n");
    std::cout << "[Multipart] Looking for header end in " << recv_buffer.size() << " bytes" << std::endl;

    if (end == std::string::npos)
    {
        std::cout << "[Multipart] Headers not complete yet" << std::endl;
        return false;
    }

    std::string headers_part = recv_buffer.substr(0, end);
    std::cout << "[Multipart] Complete headers:\n"
              << headers_part << std::endl;

    parsePartHeaders(headers_part);
    clearProcessed(end + 4);

    if (ofs.is_open())
    {
        std::cout << "[Multipart] File opened successfully, switching to READING_BODY" << std::endl;
        multipart_state_ = MultipartState::READING_BODY;
    }
    else
    {
        std::cout << "[Multipart] ERROR: File failed to open" << std::endl;
        return false;
    }
    return true;
}

// 解析请求头
void Request::parsePartHeaders(const std::string &headers_part)
{
    std::istringstream ss(headers_part);
    std::string line;
    while (std::getline(ss, line))
    {
        if (line.find("filename=") != std::string::npos)
        {
            size_t pos = line.find("filename=");
            uploaded_filename = line.substr(pos + 10);
            uploaded_filename.erase(
                remove_if(uploaded_filename.begin(), uploaded_filename.end(),
                          [](char c)
                          { return c == '/' || c == '"'; }),
                uploaded_filename.end());

            // 保存原始文件名用于验证
            std::string clean_filename = uploaded_filename;
            clean_filename.erase(std::remove(clean_filename.begin(), clean_filename.end(), '\r'), clean_filename.end());
            clean_filename.erase(std::remove(clean_filename.begin(), clean_filename.end(), '\n'), clean_filename.end());
            // 生成唯一文件名避免冲突
            auto now = std::chrono::system_clock::now();
            auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
                                 now.time_since_epoch())
                                 .count();
            std::string unique_name = std::to_string(timestamp) + "_" + clean_filename;

            std::string base_path = "/home/zhangsan/~projects/my_webserver";
            std::string uploads_dir = base_path + "/uploads";
            std::filesystem::create_directories(uploads_dir);

            upload_file_path = uploads_dir + "/" + unique_name;

            ofs.open(upload_file_path, std::ios::binary);
            if (ofs.is_open())
            {
                std::cout << "[Multipart] Opened file: " << upload_file_path
                          << " (original: " << clean_filename << ")" << std::endl;
            }
            else
            {
                std::cout << "[Multipart] ERROR: Failed to open file: " << upload_file_path
                          << ", error: " << strerror(errno) << std::endl;
            }
        }
    }
}

// 处理请求数据
bool Request::handleMultipartBody()
{
    std::cout << "[Multipart] handleMultipartBody called, buffer size: " << recv_buffer.size() << std::endl;

    // 修复：使用正确的边界标记格式
    // 在body中，边界前面有 \r\n
    std::string boundary_marker = "\r\n" + boundary_marker_;
    std::string end_boundary_marker = "\r\n" + end_boundary_marker_;

    const size_t boundary_len = boundary_marker.size();
    const size_t end_boundary_len = end_boundary_marker.size();
    const size_t max_boundary_len = std::max(boundary_len, end_boundary_len);

    std::cout << "[Multipart] Looking for boundary: " << boundary_marker << std::endl;

    // 优化：使用高效的边界查找
    size_t boundaryPos = findBoundaryOptimized(recv_buffer, boundary_marker);
    size_t endBoundaryPos = findBoundaryOptimized(recv_buffer, end_boundary_marker);

    // 优先检查结束边界
    if (endBoundaryPos != std::string::npos)
    {
        std::cout << "[Multipart] Found END boundary at position: " << endBoundaryPos << std::endl;

        // 写入结束边界之前的数据
        if (ofs.is_open() && endBoundaryPos > 0)
        {
            writeFileData(recv_buffer.data(), endBoundaryPos);
        }

        // 关闭文件并完成请求
        if (ofs.is_open())
        {
            ofs.close();
            std::cout << "[Multipart] File closed due to end boundary: " << upload_file_path << std::endl;
        }

        multipart_state_ = MultipartState::END;
        finalizeMultipart();
        clearProcessed(endBoundaryPos + end_boundary_len);
        return true;
    }

    // 检查普通边界
    if (boundaryPos != std::string::npos)
    {
        std::cout << "[Multipart] Found boundary at position: " << boundaryPos << std::endl;

        // 写入边界之前的数据
        if (ofs.is_open() && boundaryPos > 0)
        {
            writeFileData(recv_buffer.data(), boundaryPos);
        }

        // 清掉到边界为止的数据
        clearProcessed(boundaryPos + boundary_len);

        // 切换到HEADERS状态处理下一个部分
        multipart_state_ = MultipartState::READING_HEADERS;
        return true;
    }

    // 没有找到边界，写入可安全写入的数据
    // 优化：保留足够的尾部数据用于边界检查
    size_t keep_tail = max_boundary_len + 32; // 多保留一些字节
    size_t writable_size = recv_buffer.size() > keep_tail ? recv_buffer.size() - keep_tail : 0;

    std::cout << "[Multipart] No boundary found, writable_size: " << writable_size
              << ", keeping tail: " << keep_tail << std::endl;

    if (ofs.is_open() && writable_size > 0)
    {
        writeFileData(recv_buffer.data(), writable_size);
        clearProcessed(writable_size);
    }
    else if (writable_size == 0 && recv_buffer.size() > 0)
    {
        // 缓冲区太小，无法安全写入，等待更多数据
        std::cout << "[Multipart] Buffer too small for safe write, waiting for more data" << std::endl;
    }

    return false;
}

size_t Request::findBoundaryOptimized(const std::string &buffer, const std::string &pattern)
{
    if (buffer.empty() || pattern.empty())
    {
        return std::string::npos;
    }

    size_t n = buffer.size();
    size_t m = pattern.size();

    if (n < m)
    {
        return std::string::npos;
    }

    // Boyer-Moore 坏字符表
    std::vector<int> badChar(256, -1);
    for (size_t i = 0; i < m; i++)
    {
        badChar[static_cast<unsigned char>(pattern[i])] = i;
    }

    size_t s = 0;
    while (s <= n - m)
    {
        int j = m - 1;

        // 从右向左匹配
        while (j >= 0 && pattern[j] == buffer[s + j])
        {
            j--;
        }

        if (j < 0)
        {
            return s; // 找到匹配
        }
        else
        {
            // 使用坏字符规则跳过
            char mismatch_char = buffer[s + j];
            int char_pos = badChar[static_cast<unsigned char>(mismatch_char)];
            s += std::max(1, j - char_pos);
        }
    }

    return std::string::npos;
}

void Request::writeFileData(const char *data, size_t len)
{
    ofs.write(data, len);
    body_received += len;
    std::cout << "[Multipart] Wrote " << len << " bytes, total: " << body_received << " bytes" << std::endl;
}

void Request::finalizeMultipart()
{
    request_complete = true;
    if (ofs.is_open())
    {
        ofs.close();
    }
    boundary.clear(); // 重置边界，避免影响下一次请求（如果是Keep-Alive）
    std::cout << "[Multipart] Finalized, all parts processed" << std::endl;
}

void Request::clearProcessed(size_t pos)
{
    if (pos > 0 && pos <= recv_buffer.size())
        recv_buffer.erase(0, pos);
}

void Request::setBoundary(const std::string &boundary_str)
{
    raw_boundary_ = boundary_str;

    // 修复：正确的边界标记格式
    boundary_marker_ = "--" + boundary_str;            // 用于分隔部分
    end_boundary_marker_ = "--" + boundary_str + "--"; // 用于结束

    std::cout << "[Multipart] Boundary set: raw='" << raw_boundary_
              << "', marker='" << boundary_marker_
              << "', end_marker='" << end_boundary_marker_ << "'" << std::endl;
}
