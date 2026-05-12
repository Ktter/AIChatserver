#include "../../include/http/HttpResponse.h"

/*
HTTP/1.1 200 OK
Date: Tue, 15 Nov 2024 08:12:31 GMT
Server: Apache/2.4.41
Content-Type: text/html; charset=utf-8
Content-Length: 138
Connection: keep-alive

<html>
<body>
<h1>Hello, World!</h1>
</body>
</html>

┌─────────────────────────────────────┐
│         状态行 (Status Line)         │  ← 协议版本、状态码、状态描述
├─────────────────────────────────────┤
│         响应头 (Response Headers)    │  ← 键值对，描述响应信息
├─────────────────────────────────────┤
│         空行 (Empty Line)            │  ← 固定一个空行
├─────────────────────────────────────┤
│         响应体 (Response Body)       │  ← 实际的响应数据（可选）
└─────────────────────────────────────┘
*/

namespace http
{

void HttpResponse::appendToBuffer(muduo::net::Buffer* outputBuf) const
{
    // HttpResponse封装的信息格式化输出
    /*
    状态行格式: HTTP/1.1 200 OK\r\n
           └────┘ └─┘ └─┘
            版本  码  描述
            固定  固定 不定长

    HTTP/1.1 200 OK           → 短，可以用 buf[32]
    HTTP/1.1 404 Not Found    → 中等
    HTTP/1.1 418 I'm a teapot → 较长，可能超32字节！

    如果合成一个 snprintf：
    snprintf(buf, sizeof buf, "%s %d %s\r\n", 
            version, code, message);
            
    风险：statusMessage_ 可能很长，buf[32] 装不下！
    */
    char buf[32]; 
    // 为什么不把状态信息放入格式化字符串中，因为状态信息有长有短，不方便定义一个固定大小的内存存储
    // sizeof buf == sizeof(buf)
    snprintf(buf, sizeof buf, "%s %d ", httpVersion_.c_str(), statusCode_);
    
    outputBuf->append(buf);
    outputBuf->append(statusMessage_);
    outputBuf->append("\r\n");

    if (closeConnection_) // 思考一下这些地方是不是可以直接移入近headers_中
    {
        outputBuf->append("Connection: close\r\n");
    }
    else
    {
        //snprintf(buf, sizeof buf, "Content-Length: %zd\r\n", body_.size());
        //outputBuf->append(buf);
        outputBuf->append("Connection: Keep-Alive\r\n");
    }

    for (const auto& header : headers_)
    { // 为什么这里不用格式化字符串？因为key和value的长度不定
        outputBuf->append(header.first);
        outputBuf->append(": "); 
        outputBuf->append(header.second);
        outputBuf->append("\r\n");
    }
    outputBuf->append("\r\n");
    
    outputBuf->append(body_);
}

void HttpResponse::setStatusLine(const std::string& version,
                                 HttpStatusCode statusCode,
                                 const std::string& statusMessage)
{
    httpVersion_ = version;
    statusCode_ = statusCode;
    statusMessage_ = statusMessage;
}

} // namespace http