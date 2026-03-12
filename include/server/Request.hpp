#pragma once
#include <string>
#include <unordered_map>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <iostream>
#include <ctime>
#include <cstring>
#include <filesystem>

class Request
{
public:
    Request();
    // 流式追加数据
    bool appendToBuffer(const char *data, size_t len);
    // 是否保持连接
    bool isKeepAlive() const;
    // 请求是否完整
    bool isRequestComplete() const { return request_complete; }
    void reset();

    // 获取信息
    size_t getTotalReceived() const { return total_received_; } // 获取总接收量
    std::string getMethod() const { return method; }
    std::string getPath() const { return path; }
    std::string getHeaders(const std::string &key) const;
    std::string getBody() const { return body; }
    std::string getVersion() const { return version; }
    std::string getUploadedFilePath() const { return upload_file_path; }
    std::string getRecvBufffer() const { return recv_buffer; }
    std::string getUploadedFilename() const { return uploaded_filename; }
    size_t getBodyReceived() const { return body_received; }
    const std::string &getBoundaryMarker() const { return boundary_marker_; }
    void addReceivedSize(size_t size) { total_received_ += size; } // 累加接收量

private:
    // 基础字段
    std::string method;
    std::string path;
    std::string version;
    std::unordered_map<std::string, std::string> headers;
    std::string recv_buffer; // 接收缓存
    std::string body;        // 普通 body
    std::string uploaded_filename;
    std::string upload_file_path;  // 上传文件路径
    size_t expected_body_size = 0; // 预期接收
    size_t body_received = 0;      // 已接收 body
    bool request_complete = false;
    std::ofstream ofs; // 文件流（上传大文件）
    std::string boundary;
    size_t parseBoundary = 0;
    std::string raw_boundary_;    // 原始边界（从header提取）
    std::string boundary_marker_; // 统一的边界标记
    std::string end_boundary_marker_;
    size_t total_received_ = 0; // 累积整个请求的接收字节数

    // Multipart状态机
    enum class MultipartState
    {
        START,           // 寻找起始边界
        READING_HEADERS, // 读取部分头部
        READING_BODY,    // 读取文件数据
        END              // 解析完成
    };
    MultipartState multipart_state_ = MultipartState::START;
    size_t file_data_start_ = 0;
    bool in_file_data_ = false;

    // 辅助方法
    bool parseHeadersFromBuffer();
    void parseMultipartData();
    bool handleMultipartStart();
    bool handleMultipartHeaders();
    bool handleMultipartBody();
    void finalizeMultipart();

    // 文件写入和辅助工具
    void writeFileData(const char *data, size_t len);
    void parsePartHeaders(const std::string &headers);
    void clearProcessed(size_t pos);

    void setBoundary(const std::string &boundary);
    size_t findBoundaryOptimized(const std::string &buffer, const std::string &pattern);

    static inline std::string toLowerCopy(const std::string &s)
    {
        std::string r = s;
        std::transform(r.begin(), r.end(), r.begin(), [](unsigned char c)
                       { return std::tolower(c); });
        return r;
    }
};