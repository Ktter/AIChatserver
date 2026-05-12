# HTTP 服务器模块架构文档

## 一、整体架构图

```
+-----------------------------------------------------------------------------+
|                              CppAIService                                    |
|                              (AI 聊天服务器)                                  |
+-------------------------------+-----------------------------------------------+
                               |
                               v
+-----------------------------------------------------------------------------+
|                             HttpServer                                       |
|                        (基于 muduo + OpenSSL)                               |
+---------------+-------------------+-------------------+-----------------------+
               |                   |                   |
               v                   v                   v
        +--------------+    +--------------+    +--------------+
        |   Router     |    | Middleware   |    |  SSL/TLS    |
        |   (路由)     |    |  (中间件)    |    |  (加密通信)  |
        +------+-------+    +------+-------+    +------+-------+
               |                   |                   |
               v                   v                   v
        +--------------+    +--------------+    +--------------+
        |   Handler    |    |   Session    |    |   Session   |
        |   (处理器)   |    |   Manager    |    |   Storage   |
        |              |    |  (会话管理)   |    |  (会话存储)  |
        +--------------+    +--------------+    +--------------+
                               |
                               v
                        +--------------+
                        |  MySQL       |
                        |  (数据库)    |
                        +--------------+
```

---

## 二、模块清单

| 目录 | 模块 | 说明 |
|------|------|------|
| `http/` | HTTP 核心 | 请求/响应/上下文解析 |
| `router/` | 路由管理 | URL 路由匹配与分发 |
| `middleware/` | 中间件 | 请求/响应拦截处理 |
| `session/` | 会话管理 | Session 创建/存储/过期 |
| `ssl/` | SSL/TLS | HTTPS 加密通信 |
| `utils/db/` | 数据库 | MySQL 连接池与操作 |

---

## 三、核心模块详解

### 1. HTTP 核心 (http/)

```
http/
+-- HttpRequest.h/cpp     # HTTP 请求封装
+-- HttpResponse.h/cpp    # HTTP 响应封装
+-- HttpContext.h/cpp     # HTTP 上下文解析
+-- HttpServer.h/cpp      # HTTP 服务器主类
```

#### HttpRequest
- 封装 HTTP 请求信息
- 方法: `method()`, `path()`, `getHeader()`, `getBody()`
- 支持路径参数解析

#### HttpResponse
- 封装 HTTP 响应
- 方法: `setStatusCode()`, `addHeader()`, `setBody()`
- 支持 JSON/HTML 等格式

#### HttpServer
- 基于 muduo 网络库
- 处理 TCP 连接、HTTP 解析、路由分发
- 集成中间件链、SSL、Session

---

### 2. 路由管理 (router/)

```
router/
+-- Router.h/cpp          # 路由管理
+-- RouterHandler.h/cpp  # 路由处理器接口
```

#### 功能
- **精准匹配**: `GET /api/user`
- **正则匹配**: `GET /api/user/:id`
- **回调函数**: 支持 `std::function` 回调
- **对象式 Handler**: 支持 `RouterHandler` 接口

#### 匹配优先级
```
1. 精准匹配 (handlers_ / callbacks_)
2. 正则匹配 (regexHandlers_ / regexCallbacks_)
```

---

### 3. 中间件 (middleware/)

```
middleware/
+-- Middleware.h           # 抽象基类
+-- MiddlewareChain.h/cpp  # 中间件链
+-- cors/
    +-- CorsConfig.h      # CORS 配置
    +-- CorsMiddleware.h/cpp # CORS 中间件
```

#### 设计模式: 责任链模式

```
请求进入
     |
     v
+---------------------+
|  before() 链       | <- 依次处理请求
|  Logging           |
|  Auth              |
|  CORS              |
+----+---------------+
     |
     v
+---------------------+
|   Handler          | <- 业务处理
+----+---------------+
     |
     v
+---------------------+
|  after() 链        | <- 反向处理响应
|  CORS              |
|  Auth              |
+---------------------+
```

#### 核心接口
```cpp
class Middleware {
    virtual void before(HttpRequest& req) = 0;
    virtual void after(HttpResponse& resp) = 0;
};
```

---

### 4. 会话管理 (session/)

```
session/
+-- Session.h/cpp         # 会话实体
+-- SessionManager.h/cpp  # 会话管理器
+-- SessionStorage.h/cpp # 存储抽象 + 内存实现
```

#### 核心功能
- **Session 生命周期**: 创建、刷新、销毁
- **Cookie 管理**: `sessionId` 自动写入 Cookie
- **过期处理**: 默认 1 小时过期
- **存储抽象**: 支持内存/Redis/MySQL 扩展

#### 工作流程
```
Client Request
     |
     v
+-----------------+
| SessionManager  |
|  getSession()   |
+--------+--------+
         |
    +----v-----+
    | Cookie?  |
    +----v-----+
    Yes  |  No
    +----v--------+    +------------+
    | Load from   |    | Generate   |
    | Storage     |    | New Session|
    +----v--------+    +------+-----+
         |                   |
         v                   v
    +-------------------------+
    | Check isExpired()       |
    +-----------+-------------+
                |
        Expired?--Yes---> Create New
                |
               No
                v
    +-----------------------+
    | Set-Cookie in Response|
    +-----------+-----------+
                |
                v
         Return Session
```

---

### 5. SSL/TLS (ssl/)

```
ssl/
+-- SslTypes.h       # 类型定义 (枚举/错误码)
+-- SslConfig.h/cpp # SSL 配置 (证书/协议)
+-- SslContext.h/cpp# SSL 上下文 (初始化)
+-- SslConnection.h/cpp # SSL 连接 (加密通信)
```

#### SSL 握手流程
```
TCP 连接建立
     |
     v
startHandshake()
     |
     v
+---------------------------------+
|  SSL_do_handshake()            |
|  - ClientHello                  |
|  - ServerHello + 证书            |
|  - 密钥交换                     |
|  - 加密确认                     |
+----------------+----------------+
                  |
                  v 握手成功
+---------------------------------+
|  ESTABLISHED (加密通信状态)     |
|  - send(): 加密发送             |
|  - onRead(): 解密接收           |
+---------------------------------+
```

#### 配置项
| 配置 | 说明 | 默认值 |
|------|------|--------|
| `certFile` | 证书文件 | - |
| `keyFile` | 私钥文件 | - |
| `version` | TLS 版本 | TLS_1_2 |
| `cipherList` | 加密套件 | HIGH:!aNULL:!MDS |
| `sessionTimeout` | 会话超时 | 300s |

---

### 6. 数据库 (utils/db/)

```
utils/
+-- db/
|   +-- DbConnection.h/cpp     # 单个数据库连接
|   +-- DbConnectionPool.h/cpp  # 连接池
|   +-- DbException.h           # 异常类
+-- MysqlUtil.h                # MySQL 工具类
+-- JsonUtil.h                 # JSON 工具类
+-- FileUtil.h                 # 文件工具类
```

#### 连接池特性
- **预创建连接**: 初始化时创建 `initialSize_` 个连接
- **连接复用**: `getConnection()` 获取，用完自动归还
- **健康检查**: 后台线程定期 `ping()` 检查连接
- **自动重连**: 断连后自动重连

#### SQL 操作封装
```cpp
// 查询
auto res = mysqlUtil_.executeQuery(
    "SELECT id, name FROM users WHERE id = ?", 
    userId
);

// 更新
mysqlUtil_.executeUpdate(
    "INSERT INTO users (name, email) VALUES (?, ?)",
    name, email
);
```

---

## 四、请求处理流程

```
+-----------------------------------------------------------------------------+
|                              请求处理流程                                     |
+-----------------------------------------------------------------------------+

  客户端                  HttpServer                 Router              Handler
    |                        |                        |                    |
    |---- TCP 连接 --------->|                        |                    |
    |                        |                        |                    |
    |---- HTTP 请求 -------->|                        |                    |
    |                        |                        |                    |
    |                        |---> MiddlewareChain     |                    |
    |                        |    processBefore()      |                    |
    |                        |    - CORS 检查          |                    |
    |                        |                        |                    |
    |                        |---> SessionManager     |                    |
    |                        |    getSession()         |                    |
    |                        |    - 获取/创建 Session  |                    |
    |                        |                        |                    |
    |                        |---> Router.route()     |                    |
    |                        |    - 匹配 URL           |                    |
    |                        |                        |                    |
    |                        |----------------------->|---> 业务处理      |
    |                        |                        |                    |
    |                        |<-----------------------|<--- 返回结果        |
    |                        |                        |                    |
    |                        |---> MiddlewareChain     |                    |
    |                        |    processAfter()      |                    |
    |                        |    - 添加 CORS 头      |                    |
    |<--- HTTP 响应 ---------|                        |                    |
    |                        |                        |                    |
    |---- TCP 关闭 ---------|                        |                    |
```

---

## 五、关键技术点

### 1. 智能指针管理资源
```cpp
// 连接池返回智能指针，用完自动归还
return std::shared_ptr<DbConnection>(conn.get(), 
    [this](DbConnection*) {
        // 归还连接
        connections_.push(conn);
    });
```

### 2. 模板可变参数实现 SQL 参数绑定
```cpp
template<typename... Args>
sql::ResultSet* executeQuery(const std::string& sql, Args&&... args) {
    auto stmt = conn_->prepareStatement(sql);
    bindParams(stmt.get(), 1, std::forward<Args>(args)...);
    return stmt->executeQuery();
}
```

### 3. 中间件责任链
```cpp
// before() 正向遍历，after() 反向遍历
for (auto& m : middlewares_) m->before(req);
handler->handle(req, resp);
for (auto it = rbegin(); it != rend(); ++it) (*it)->after(resp);
```

### 4. SSL BIO 自定义
```cpp
// 使用内存 BIO 实现加密数据中转
SSL_set_bio(ssl_, readBio_, writeBio_);
// readBio_ <- 网络数据
// SSL_read() <- readBio_ 解密
// writeBio_ <- SSL_write() 加密
// conn_->send() -> 网络发送
```

### 5. Session + Cookie
```cpp
// 登录成功后设置 Cookie
std::string cookie = "sessionId=" + sessionId + ";Only";
resp-> Path=/; HttpaddHeader("Set-Cookie", cookie);
```

---

## 六、扩展点

### 添加新中间件
```cpp
class LoggingMiddleware : public Middleware {
    void before(HttpRequest& req) override {
        LOG_INFO << req.methodString() << " " << req.path();
    }
    void after(HttpResponse& resp) override {
        LOG_INFO << "Response: " << resp.statusCode();
    }
};
// 注册
httpServer_.addMiddleware(std::make_shared<LoggingMiddleware>());
```

### 添加新路由
```cpp
router_.registerHandler(HttpRequest::kGet, "/api/user", 
    std::make_shared<UserHandler>());

// 或回调函数
router_.registerCallback(HttpRequest::kGet, "/api/hello", 
    [](const HttpRequest& req, HttpResponse* resp) {
        resp->setBody("Hello!");
    });
```

### 扩展 Session 存储
```cpp
class RedisSessionStorage : public SessionStorage {
    void save(shared_ptr<Session>) override;
    shared_ptr<Session> load(const string&) override;
    void remove(const string&) override;
};
```

---

## 七、总结

本项目 HTTP 模块是一个**轻量级 Web 框架**，具备：

| 特性 | 实现 |
|------|------|
| **网络通信** | 基于 muduo |
| **路由** | 精准匹配 + 正则匹配 |
| **中间件** | 责任链模式 |
| **会话** | Session/Cookie 管理 |
| **安全** | SSL/TLS 加密 |
| **数据库** | MySQL 连接池 |

模块之间**低耦合**，通过接口通信，便于扩展和维护。
