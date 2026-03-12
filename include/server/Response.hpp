#pragma once
#include <string>
#include <unordered_map>
#include <sstream>

// Http状态码
const std::unordered_map<int, std::string> STATUS_MAP = {
    {200, "ok"},
    {400, "Bad Request"},
    {404, "Not Found"},
    {500, "Server Error"},
    {405, "Method Not Found"}};

class Response
{
public:
    Response() : status_code(200), version("HTTP/1.1")
    {
        // 设置默认响应头（如内容类型、连接方式）
        setHeader("Content-Type", "text/html; charset=utf-8");
        // setHeader("Connection", "keep-alive"); // 支持长链接
    };

    // 设置响应属性
    void setStatusCode(int code) { status_code = code; }
    void setVersion(std::string v) { version = v; }
    void setHeader(const std::string &key, const std::string &value) { headers[key] = value; }
    void setBody(const std::string &b)
    {
        body = b;
        // 自动设置 Content-Length（避免客户端读取不完整）
        setHeader("Content-Length", std::to_string(b.size()));
    }

    // 生成最终的响应字符串（供 Socket 发送）
    std::string toString() const;
    void reset();

private:
    int status_code;                                      // 状态码
    std::string version;                                  // HTTP版本
    std::unordered_map<std::string, std::string> headers; // 响应头
    std::string body;                                     // 响应体
};