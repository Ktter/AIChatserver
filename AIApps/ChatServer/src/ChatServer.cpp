#include "../include/handlers/ChatLoginHandler.h"
#include "../include/handlers/ChatRegisterHandler.h"
#include "../include/handlers/ChatLogoutHandler.h"
#include "../include/handlers/ChatHandler.h"
#include "../include/handlers/ChatEntryHandler.h"
#include "../include/handlers/ChatSendHandler.h"
#include "../include/handlers/AIMenuHandler.h"
#include "../include/handlers/AIUploadSendHandler.h"
#include "../include/handlers/AIUploadHandler.h"
#include "../include/handlers/ChatHistoryHandler.h"
#include "../include/handlers/ChatCreateAndSendHandler.h"
#include "../include/handlers/ChatSessionsHandler.h"
#include "../include/handlers/ChatSpeechHandler.h"

#include "../include/ChatServer.h"
#include "../../../HttpServer/include/http/HttpRequest.h"
#include "../../../HttpServer/include/http/HttpResponse.h"
#include "../../../HttpServer/include/http/HttpServer.h"

using namespace http;

/**
 * @brief ChatServer 构造函数
 *
 * 初始化 HTTP 服务器并调用 initialize() 完成全部初始化工作
 *
 * @param port 服务器监听端口
 * @param name 服务器名称
 * @param option muduo TcpServer 选项
 */
ChatServer::ChatServer(int port,
    const std::string& name,
    muduo::net::TcpServer::Option option)
    : httpServer_(port, name, option)
{
    initialize();
}

/**
 * @brief ChatServer 初始化入口
 *
 * 执行完整的服务器初始化流程，包括：
 * - MySQL 数据库连接初始化
 * - 会话管理器初始化
 * - 中间件初始化
 * - 路由注册
 */
void ChatServer::initialize() {
    std::cout << "ChatServer initialize start  ! " << std::endl;
    http::MysqlUtil::init("tcp://127.0.0.1:3306", "debian-sys-maint", "R51Uw3Rue2T9oO2B", "ChatHttpServer", 5);

    initializeSession();
    initializeMiddleware();
    initializeRouter();
}

/**
 * @brief 初始化聊天消息数据
 *
 * 从 MySQL 数据库加载历史聊天记录，用于服务重启后恢复会话状态
 *
 * @see readDataFromMySQL()
 */
void ChatServer::initChatMessage() {
    std::cout << "initChatMessage start ! " << std::endl;
    readDataFromMySQL();
    std::cout << "initChatMessage success ! " << std::endl;
}

/**
 * @brief 从 MySQL 数据库读取聊天历史记录
 *
 * 查询 chat_message 表，按时间戳和ID排序，加载所有历史消息到内存。
 * 每个用户的会话消息被恢复到对应的 AIHelper 对象中。
 *
 * @post 将历史消息加载到 chatInformation 和 sessionsIdsMap 中
 */
void ChatServer::readDataFromMySQL() {
    std::string sql = "SELECT id, username,session_id, is_user, content, ts FROM chat_message ORDER BY ts ASC, id ASC";

    sql::ResultSet* res;
    try {
        res = mysqlUtil_.executeQuery(sql);
    }
    catch (const std::exception& e) {
        std::cerr << "MySQL query failed: " << e.what() << std::endl;
        return;
    }

    while (res->next()) {
        long long user_id = 0;
        std::string session_id;
        std::string username, content;
        long long ts = 0;
        int is_user = 1;

        try {
            user_id    = res->getInt64("id");
            session_id = res->getString("session_id");
            username   = res->getString("username");
            content    = res->getString("content");
            ts         = res->getInt64("ts");
            is_user    = res->getInt("is_user");
        }
        catch (const std::exception& e) {
            std::cerr << "Failed to read row: " << e.what() << std::endl;
            continue;
        }

        auto& userSessions = chatInformation[user_id];

        std::shared_ptr<AIHelper> helper;
        auto itSession = userSessions.find(session_id);
        if (itSession == userSessions.end()) {
            helper = std::make_shared<AIHelper>();
            userSessions[session_id] = helper;
            sessionsIdsMap[user_id].push_back(session_id);
        } else {
            helper = itSession->second;
        }

        helper->restoreMessage(content, ts);
    }

    std::cout << "readDataFromMySQL finished" << std::endl;
}

/**
 * @brief 设置 HTTP 服务器的工作线程数
 *
 * 底层调用 muduo TcpServer::setThreadNum()，用于调整并发处理能力
 *
 * @param numThreads 工作线程数量
 * @see start()
 */
void ChatServer::setThreadNum(int numThreads) {
    httpServer_.setThreadNum(numThreads);
}

/**
 * @brief 启动 HTTP 服务器
 *
 * 调用底层 muduo TcpServer::start()，开始监听并处理 HTTP 请求
 *
 * @pre 服务器应已完成初始化 (initialize())
 * @post 服务器开始接受客户端连接
 * @see setThreadNum()
 */
void ChatServer::start() {
    httpServer_.start();
}

/**
 * @brief 注册 HTTP 路由及对应的请求处理器
 *
 * 将 URL 路径与对应的 Handler 进行绑定，支持 GET/POST 方法。
 * 包括页面路由(入口、聊天、上传)和 API 路由(登录、注册、发送消息等)
 *
 * @post 所有 HTTP 路由注册完成，可处理客户端请求
 * @see initialize()
 */
void ChatServer::initializeRouter() {
    // 主页/入口点，返回聊天界面 HTML
    httpServer_.Get("/", std::make_shared<ChatEntryHandler>(this));
    // 聊天入口页面（别名）
    httpServer_.Get("/entry", std::make_shared<ChatEntryHandler>(this));

    // 用户登录接口，验证用户名密码，返回会话
    httpServer_.Post("/login", std::make_shared<ChatLoginHandler>(this));

    // 用户注册接口，创建新用户账号
    httpServer_.Post("/register", std::make_shared<ChatRegisterHandler>(this));

    // 用户登出接口，销毁会话
    httpServer_.Post("/user/logout", std::make_shared<ChatLogoutHandler>(this));

    // 获取聊天页面/历史消息
    httpServer_.Get("/chat", std::make_shared<ChatHandler>(this));

    // 发送聊天消息（文本），返回 AI 回复
    httpServer_.Post("/chat/send", std::make_shared<ChatSendHandler>(this));

    // 获取 AI 模型菜单（可用模型列表）
    httpServer_.Get("/menu", std::make_shared<AIMenuHandler>(this));

    // 文件上传页面
    httpServer_.Get("/upload", std::make_shared<AIUploadHandler>(this));

    // 上传文件并发送消息（支持图片/文档）
    httpServer_.Post("/upload/send", std::make_shared<AIUploadSendHandler>(this));

    // 获取指定会话的聊天历史记录
    httpServer_.Post("/chat/history", std::make_shared<ChatHistoryHandler>(this));

    // 创建新会话并发送消息（支持多会话管理）
    httpServer_.Post("/chat/send-new-session", std::make_shared<ChatCreateAndSendHandler>(this));
    // 获取用户所有会话列表
    httpServer_.Get("/chat/sessions", std::make_shared<ChatSessionsHandler>(this));

    // 文字转语音接口，将文本转换为语音
    httpServer_.Post("/chat/tts", std::make_shared<ChatSpeechHandler>(this));
}

/**
 * @brief 初始化会话管理器
 *
 * 创建基于内存的会话存储 (MemorySessionStorage) 和会话管理器 (SessionManager)，
 * 用于管理用户的登录状态和会话数据
 *
 * @post 会话管理器设置完成，可处理用户会话
 * @see initialize()
 */
void ChatServer::initializeSession() {
    auto sessionStorage = std::make_unique<http::session::MemorySessionStorage>();
    auto sessionManager = std::make_unique<http::session::SessionManager>(std::move(sessionStorage));
    setSessionManager(std::move(sessionManager));
}

/**
 * @brief 初始化 HTTP 中间件
 *
 * 添加 CORS (Cross-Origin Resource Sharing) 中间件，支持跨域请求
 *
 * @post 中间件注册完成，HTTP 服务器将应用 CORS 策略
 * @see initialize()
 */
void ChatServer::initializeMiddleware() {
    auto corsMiddleware = std::make_shared<http::middleware::CorsMiddleware>();
    httpServer_.addMiddleware(corsMiddleware);
}

/**
 * @brief 封装 HTTP 响应消息
 *
 * 统一设置 HTTP 响应的各个字段，包括版本、状态、头信息、body 等。
 * 异常时自动设置 500 状态码并关闭连接
 *
 * @param version HTTP 协议版本 (如 "HTTP/1.1")
 * @param statusCode HTTP 状态码 (如 200, 404, 500)
 * @param statusMsg 状态描述信息 (如 "OK", "Not Found")
 * @param close 是否关闭连接
 * @param contentType 内容类型 (如 "text/html", "application/json")
 * @param contentLen 内容长度
 * @param body 响应体内容
 * @param resp HttpResponse 对象指针，用于填充响应数据
 * @pre resp 不应为 nullptr
 * @post resp 对象包含完整的 HTTP 响应信息
 */
void ChatServer::packageResp(const std::string& version,
    http::HttpResponse::HttpStatusCode statusCode,
    const std::string& statusMsg,
    bool close,
    const std::string& contentType,
    int contentLen,
    const std::string& body,
    http::HttpResponse* resp)
{
    if (resp == nullptr)
    {
        LOG_ERROR << "Response pointer is null";
        return;
    }

    try
    {
        resp->setVersion(version);
        resp->setStatusCode(statusCode);
        resp->setStatusMessage(statusMsg);
        resp->setCloseConnection(close);
        resp->setContentType(contentType);
        resp->setContentLength(contentLen);
        resp->setBody(body);

        LOG_INFO << "Response packaged successfully";
    }
    catch (const std::exception& e)
    {
        LOG_ERROR << "Error in packageResp: " << e.what();

        resp->setStatusCode(http::HttpResponse::k500InternalServerError);
        resp->setStatusMessage("Internal Server Error");
        resp->setCloseConnection(true);
    }
}
