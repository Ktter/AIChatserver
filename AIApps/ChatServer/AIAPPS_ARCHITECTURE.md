# ChatServer 模块架构文档

## 一、整体架构图

```
+-----------------------------------------------------------------------------+
|                             CppAIService                                    |
|                             (AI 聊天服务器)                                  |
+-------------------------------+---------------------------------------------+
                               |
                               v
+-----------------------------------------------------------------------------+
|                            ChatServer                                       |
|                     (继承自 HttpServer)                                     |
+------------------+------------------+------------------+--------------------+
|                  |                  |                  |                    |
|    Handlers     |    AI Util       |    业务数据      |    外部依赖         |
|    (处理器)      |    (AI工具)       |                  |                    |
+--------+---------+--------+----------+---------+-------+                    |
|        |          |        |         |         |       |                    |
+--------v---------v--------v---------v---------v-------+--------------------+
|                              请求入口                                        |
|  /login  /register  /chat/send  /upload/send  /chat/tts  ...              |
+-----------------------------------------------------------------------------+
```

---

## 二、目录结构

```
ChatServer/
├── include/
│   ├── ChatServer.h           # 主服务器类
│   ├── handlers/              # HTTP 请求处理器
│   │   ├── ChatLoginHandler.h
│   │   ├── ChatRegisterHandler.h
│   │   ├── ChatLogoutHandler.h
│   │   ├── ChatHandler.h
│   │   ├── ChatEntryHandler.h
│   │   ├── ChatSendHandler.h
│   │   ├── ChatHistoryHandler.h
│   │   ├── ChatSessionsHandler.h
│   │   ├── ChatCreateAndSendHandler.h
│   │   ├── AIMenuHandler.h
│   │   ├── AIUploadHandler.h
│   │   ├── AIUploadSendHandler.h
│   │   └── ChatSpeechHandler.h
│   └── AIUtil/               # AI 工具类
│       ├── AIHelper.h        # AI 对话封装 (curl 调用阿里云)
│       ├── AIConfig.h        # AI 配置
│       ├── AIStrategy.h      # 策略模式: 多模型支持
│       ├── AIFactory.h       # 工厂模式: 创建策略
│       ├── AIToolRegistry.h  # 工具注册
│       ├── ImageRecognizer.h # 图像识别
│       ├── AISpeechProcessor.h # 语音处理
│       ├── MQManager.h       # 消息队列管理
│       └── base64.h          # Base64 编解码
└── src/
    ├── main.cpp
    ├── ChatServer.cpp
    ├── handlers/             # 处理器实现
    └── AIUtil/              # AI 工具实现
```

---

## 三、路由清单

| 方法 | 路径 | Handler | 功能 |
|------|------|---------|------|
| GET | `/` | ChatEntryHandler | 返回聊天界面 HTML |
| GET | `/entry` | ChatEntryHandler | 聊天入口页面 |
| POST | `/login` | ChatLoginHandler | 用户登录 |
| POST | `/register` | ChatRegisterHandler | 用户注册 |
| POST | `/user/logout` | ChatLogoutHandler | 用户登出 |
| GET | `/chat` | ChatHandler | 获取聊天页面 |
| POST | `/chat/send` | ChatSendHandler | 发送聊天消息 |
| GET | `/menu` | AIMenuHandler | 获取 AI 模型菜单 |
| GET | `/upload` | AIUploadHandler | 文件上传页面 |
| POST | `/upload/send` | AIUploadSendHandler | 上传文件并发送 |
| POST | `/chat/history` | ChatHistoryHandler | 获取聊天历史 |
| POST | `/chat/send-new-session` | ChatCreateAndSendHandler | 创建新会话并发送 |
| GET | `/chat/sessions` | ChatSessionsHandler | 获取会话列表 |
| POST | `/chat/tts` | ChatSpeechHandler | 文字转语音 |

---

## 四、核心模块详解

### 1. ChatServer 主类

**继承关系**: `ChatServer` extends `HttpServer`

**核心成员**:
```cpp
http::HttpServer         httpServer_;           // HTTP 服务器
http::MysqlUtil          mysqlUtil_;            // MySQL 工具

// 用户状态管理
std::unordered_map<int, bool>              onlineUsers_;      // 在线用户

// AI 对话管理 (用户ID -> 会话ID -> AIHelper)
std::unordered_map<int, std::unordered_map<std::string, std::shared_ptr<AIHelper>>>
                                                chatInformation_;

// 图像识别管理
std::unordered_map<int, std::shared_ptr<ImageRecognizer>>  ImageRecognizerMap_;

// 会话列表管理
std::unordered_map<int, std::vector<std::string>>  sessionsIdsMap_;
```

**初始化流程**:
```cpp
void ChatServer::initialize() {
    MysqlUtil::init(...);      // 初始化 MySQL
    initializeSession();       // 初始化 Session
    initializeMiddleware();    // 初始化中间件 (CORS)
    initializeRouter();         // 注册路由
}
```

### 2. Handlers (HTTP 处理器)

每个 Handler 继承自 `http::router::RouterHandler`

```
RouterHandler (接口)
    │
    ├── handle(req, resp) = 0
    │
    ▼
ChatLoginHandler     ── 登录验证，查询用户
ChatRegisterHandler ── 用户注册，插入数据
ChatLogoutHandler   ── 登出，销毁 Session
ChatHandler         ── 返回聊天页面
ChatSendHandler     ── 发送消息，调用 AI
ChatHistoryHandler  ── 查询历史消息
ChatSessionsHandler ── 获取用户会话列表
AIMenuHandler       ── 返回可用 AI 模型
AIUploadHandler     ── 返回上传页面
AIUploadSendHandler ── 处理文件上传 + AI 分析
ChatSpeechHandler   ── 文字转语音 (TTS)
```

### 3. AIUtil (AI 工具)

#### AIHelper - AI 对话封装
```cpp
class AIHelper {
    std::shared_ptr<AIStrategy> strategy;  // AI 策略
    
    // 消息历史 [内容, 时间戳]
    std::vector<std::pair<std::string, long long>> messages;
    
    // 添加消息
    void addMessage(int userId, const std::string& userName, 
                    bool is_user, const std::string& userInput, 
                    std::string sessionId);
    
    // 发送聊天，返回 AI 回复
    std::string chat(int userId, std::string userName, 
                     std::string sessionId, std::string userQuestion, 
                     std::string modelType);
    
    // 执行 curl 请求
    json executeCurl(const json& payload);
};
```

#### AIStrategy - 策略模式 (多模型支持)
```
AIStrategy (抽象基类)
    │
    ├── getApiUrl()
    ├── getApiKey()
    ├── getModel()
    ├── buildRequest()
    └── parseResponse()
    │
    ├── AliyunStrategy       ── 阿里云通义千问
    ├── DouBaoStrategy        ── 字节豆包
    ├── AliyunRAGStrategy     ── 阿里云 RAG
    └── AliyunMcpStrategy     ── 阿里云 MCP
```

---

## 五、数据流示意图

### 1. 整体数据流

```
┌─────────────────────────────────────────────────────────────────────────────────────────────┐
│                                    客户端 (浏览器/APP)                                     │
│                                                                                          │
│   ┌─────────┐    ┌─────────┐    ┌─────────┐    ┌─────────┐    ┌─────────┐             │
│   │  登录   │    │  注册   │    │  发送   │    │  上传   │    │  TTS   │             │
│   │ 页面    │    │ 页面    │    │ 消息    │    │ 图片    │    │ 请求   │             │
│   └────┬────┘    └────┬────┘    └────┬────┘    └────┬────┘    └────┬────┘             │
│        │              │              │              │              │                    │
│        └──────────────┴──────────────┴──────────────┴──────────────┘                    │
│                                      │                                                  │
└──────────────────────────────────────┼──────────────────────────────────────────────────┘
                                       │ HTTP/HTTPS
                                       ▼
┌─────────────────────────────────────────────────────────────────────────────────────────────┐
│                                   ChatServer                                              │
│                                                                                          │
│   ┌─────────────────────────────────────────────────────────────────────────────┐         │
│   │                           HttpServer (muduo)                                 │         │
│   │   ┌─────────────┐    ┌─────────────┐    ┌─────────────┐    ┌───────────┐  │         │
│   │   │   TCP       │    │  HTTP       │    │ Middleware  │    │  Session  │  │         │
│   │   │  Accept     │───▶│  Parse      │───▶│  Chain     │───▶│  Manager  │  │         │
│   │   └─────────────┘    └─────────────┘    └─────────────┘    └───────────┘  │         │
│   │                                                    │                              │         │
│   │                                                    ▼                              │         │
│   │   ┌──────────────────────────────────────────────────────────────────────┐    │         │
│   │   │                                Router                                  │    │         │
│   │   │   /login ──▶ LoginHandler   /chat/send ──▶ SendHandler              │    │         │
│   │   │   /register ─▶ RegisterH   /upload/send ──▶ UploadSendHandler     │    │         │
│   │   └──────────────────────────────────────────────────────┬───────┘         │    │         │
│   └──────────────────────────────────────────────────────────┼──────────────────┘         │
│                                                              │                            │
└──────────────────────────────────────────────────────────────┼────────────────────────────┘
                                                               │
                                                               ▼
┌─────────────────────────────────────────────────────────────────────────────────────────────┐
│                                    Handler 处理层                                          │
│                                                                                          │
│   ┌─────────────────┐    ┌─────────────────┐    ┌─────────────────┐                    │
│   │  LoginHandler   │    │  SendHandler    │    │ UploadSendHandler│                   │
│   │  (登录处理)      │    │  (发送消息)     │    │  (上传处理)      │                   │
│   └────────┬────────┘    └────────┬────────┘    └────────┬────────┘                    │
│            │                      │                      │                             │
│            ▼                      ▼                      ▼                             │
│   ┌─────────────────┐    ┌─────────────────┐    ┌─────────────────┐                    │
│   │  MySQL 查询      │    │   AIHelper     │    │ ImageRecognizer │                    │
│   │  users 表        │    │   (AI 对话)    │    │   (图像识别)    │                    │
│   └─────────────────┘    └────────┬────────┘    └────────┬────────┘                    │
│                                   │                      │                             │
│                                   ▼                      ▼                             │
│                          ┌─────────────────┐    ┌─────────────────┐                    │
│                          │   AIHelper      │    │   AIHelper      │                    │
│                          │  chat()         │    │  chat()         │                    │
│                          └────────┬────────┘    └────────┬────────┘                    │
│                                   │                      │                             │
└───────────────────────────────────┼──────────────────────┼──────────────────────────────┘
                                    │                      │
                                    ▼                      ▼
                         ┌─────────────────────┐   ┌─────────────────────┐
                         │   AI Strategy      │   │   外部 AI 服务      │
                         │   (策略模式)        │   │                     │
                         ├─────────────────────┤   │  ┌───────────────┐  │
                         │ AliyunStrategy     │   │  │  阿里云 DashScope│  │
                         │ DouBaoStrategy     │   │  │  字节豆包       │  │
                         │ AliyunRAGStrategy │   │  │  自定义模型     │  │
                         │ AliyunMcpStrategy │   │  └───────────────┘  │
                         └─────────┬─────────┘   └─────────────────────┘
                                   │
                                   ▼
                         ┌─────────────────────┐
                         │   curl HTTP 请求     │
                         │   (调用 AI API)      │
                         └─────────┬───────────┘
                                   │
                                   ▼
                         ┌─────────────────────┐
                         │   AI API 返回       │
                         │   (JSON 响应)       │
                         └─────────┬───────────┘
                                   │
                                   ▼
                         ┌─────────────────────┐
                         │   Strategy 解析      │
                         │   parseResponse()   │
                         └─────────┬───────────┘
                                   │
                                   ▼
                         ┌─────────────────────┐
                         │    返回 AI 回复     │
                         └─────────────────────┘
```

### 2. 用户发送消息完整流程（12步）

```
用户发送: "你好，请帮我解释一下C++"
│
│  ┌────────────────────────────────────────────────────────────────────┐
│  │ 1. HTTP 请求进入                                                   │
│  │    POST /chat/send                                                │
│  │    Body: {"message": "你好...", "sessionId": "xxx", "model": "aliyun"} │
│  └────────────────────────────────────────────────────────────────────┘
│                                    │
│                                    ▼
│  ┌────────────────────────────────────────────────────────────────────┐
│  │ 2. Middleware 处理                                                 │
│  │    - CORS 检查 ( OPTIONS 请求处理 )                                 │
│  │    - 添加 CORS 响应头                                              │
│  └────────────────────────────────────────────────────────────────────┘
│                                    │
│                                    ▼
│  ┌────────────────────────────────────────────────────────────────────┐
│  │ 3. Session 管理                                                    │
│  │    - 从 Cookie 获取 sessionId                                      │
│  │    - 获取用户ID                                                    │
│  └────────────────────────────────────────────────────────────────────┘
│                                    │
│                                    ▼
│  ┌────────────────────────────────────────────────────────────────────┐
│  │ 4. ChatSendHandler::handle()                                      │
│  │    - 解析请求参数 (message, sessionId, model)                    │
│  │    - 获取用户 ID                                                   │
│  └────────────────────────────────────────────────────────────────────┘
│                                    │
│                                    ▼
│  ┌────────────────────────────────────────────────────────────────────┐
│  │ 5. 获取/创建 AIHelper                                              │
│  │    - chatInformation_[userId][sessionId]                          │
│  │    - 不存在则创建新的 AIHelper                                      │
│  └────────────────────────────────────────────────────────────────────┘
│                                    │
│                                    ▼
│  ┌────────────────────────────────────────────────────────────────────┐
│  │ 6. AIHelper::chat()                                               │
│  │    a) addMessage() - 添加用户消息到历史                            │
│  │    b) 构造请求 JSON                                                │
│  └────────────────────────────────────────────────────────────────────┘
│                                    │
│                                    ▼
│  ┌────────────────────────────────────────────────────────────────────┐
│  │ 7. AIHelper::executeCurl()                                        │
│  │    - 调用阿里云 DashScope API                                       │
│  │    - POST https://dashscope.aliyuncs.com/compatible-mode/v1/...   │
│  └────────────────────────────────────────────────────────────────────┘
│                                    │
│                                    ▼
│  ┌────────────────────────────────────────────────────────────────────┐
│  │ 8. 阿里云返回 JSON                                                 │
│  │    {"choices": {"message": {"content": "C++是一种..."}}}          │
│  └────────────────────────────────────────────────────────────────────┘
│                                    │
│                                    ▼
│  ┌────────────────────────────────────────────────────────────────────┐
│  │ 9. Strategy::parseResponse()                                      │
│  │    - 提取 content 字段                                             │
│  └────────────────────────────────────────────────────────────────────┘
│                                    │
│                                    ▼
│  ┌────────────────────────────────────────────────────────────────────┐
│  │ 10. AIHelper::addMessage()                                        │
│      - 添加 AI 回复到消息历史                                           │
│  └────────────────────────────────────────────────────────────────────┘
│                                    │
│                                    ▼
│  ┌────────────────────────────────────────────────────────────────────┐
│  │ 11. 持久化到 MySQL                                                 │
│      - INSERT INTO chat_message (...)                                  │
│  └────────────────────────────────────────────────────────────────────┘
│                                    │
│                                    ▼
│  ┌────────────────────────────────────────────────────────────────────┐
│  │ 12. 返回响应                                                       │
│      {"code": 200, "data": {"reply": "C++是...", "sessionId": "xxx"}} │
│  └────────────────────────────────────────────────────────────────────┘
│                                    │
│                                    ▼
                              用户收到 AI 回复
```

### 3. 模块间数据传递

```
┌─────────────┐     json      ┌─────────────┐     json     ┌─────────────┐
│  Handler    │ ────────────▶ │  AIHelper   │ ──────────▶ │  Strategy   │
│             │   req.body    │             │  buildReq()  │             │
│ - 解析请求  │               │ - 添加历史  │              │ - 阿里云    │
│ - 业务逻辑  │               │ - curl调用  │              │ - 豆包      │
│ - 返回响应  │               │             │              │ - RAG       │
└──────┬──────┘               └──────┬──────┘              └──────┬──────┘
       │                             │                            │
       ▼                             ▼                            ▼
┌─────────────┐              ┌─────────────┐              ┌─────────────┐
│   MySQL     │              │     curl    │              │   AI API   │
│             │              │             │              │             │
│ - users     │              │ - HTTP 请求 │              │ - DashScope│
│ - messages  │              │ - HTTPS     │              │ - 火山引擎 │
└─────────────┘              └─────────────┘              └─────────────┘
```

---

## 六、核心技术点

### 1. 策略模式 + 工厂模式

```cpp
// 工厂创建策略
std::shared_ptr<AIStrategy> AIFactory::createStrategy(const std::string& type) {
    if (type == "aliyun") return std::make_shared<AliyunStrategy>();
    if (type == "doubao") return std::make_shared<DouBaoStrategy>();
    // ...
}

// AIHelper 使用策略
void AIHelper::setStrategy(std::shared_ptr<AIStrategy> strat) {
    strategy = strat;
}
```

### 2. Session 管理

```cpp
// ChatServer 初始化 Session
auto sessionStorage = std::make_unique<MemorySessionStorage>();
auto sessionManager = std::make_unique<SessionManager>(std::move(sessionStorage));
setSessionManager(std::move(sessionManager));
```

### 3. 多模型支持

```cpp
// 通过 model 参数选择模型
std::string chat(std::string userQuestion, std::string modelType) {
    auto strategy = AIFactory::createStrategy(modelType);
    setStrategy(strategy);
    // ...
}
```

---

## 七、学习路径建议

### 入门 (先看这 3 个文件)

| 文件 | 重点 |
|------|------|
| `ChatServer.h` | 了解整体结构、成员变量 |
| `ChatServer.cpp` | 了解初始化流程、路由注册 |
| `ChatSendHandler.cpp` | 了解消息处理流程 |

### 进阶 (理解核心逻辑)

| 模块 | 文件 | 重点 |
|------|------|------|
| AI 对话 | `AIHelper.cpp/h` | curl 调用、AI 响应解析 |
| 多模型 | `AIStrategy.cpp/h` | 策略模式实现 |
| 图像 | `ImageRecognizer.cpp` | 图片上传与识别 |
| 语音 | `AISpeechProcessor.cpp` | TTS 实现 |

---

## 八、总结

ChatServer 模块特点：

| 特性 | 实现 |
|------|------|
| **架构** | 继承 HttpServer，复用路由/中间件/Session |
| **AI 调用** | 策略模式支持多模型 (阿里云/字节/自定义) |
| **多会话** | 每个用户多个会话，会话历史存储 |
| **功能** | 文本/图片/语音 三合一 |
| **存储** | MySQL 持久化聊天记录 |

模块之间**低耦合**，通过 ChatServer 统一管理 AIHelper 和数据。
