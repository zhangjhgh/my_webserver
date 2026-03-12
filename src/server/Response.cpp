#include "server/Response.hpp"

std::string Response::toString() const
{
    std::string resp;

    // 1. 响应行（必须是 "HTTP/1.1 状态码 描述\r\n"）
    auto it = STATUS_MAP.find(status_code);
    std::string status_msg = it->second;
    resp += "HTTP/1.1 " + std::to_string(status_code) + " " + status_msg + "\r\n";

    // 2. 响应头（每个头格式："Key: Value\r\n"）
    for (const auto &[key, value] : headers)
    {
        resp += key + ": " + value + "\r\n";
    }

    // 3. 响应头和响应体之间必须加一个空行（\r\n）
    resp += "\r\n";

    // 4. 响应体
    resp += body;

    return resp;
}

void Response::reset()
{
    status_code = 200; // 默认状态码
    headers.clear();   // 清空 headers
    body.clear();      // 清空 body
}