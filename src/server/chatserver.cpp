#include "chatserver.hpp"
#include "json.hpp"

#include "chatservice.hpp"

#include <string>
#include <functional>
using namespace std;
using namespace placeholders;
using json = nlohmann::json;


ChatServer::ChatServer(EventLoop* loop, const InetAddress& listenAddr, const string& nameArg):   // 事件循环  IP+Port   服务器的名字
        _server(loop, listenAddr, nameArg),
        _loop(loop)
{
    // 给服务器注册用户连接的创建和断开回调（相应事件发生后，自动调用回调函数，但我不知道什么时候发生）
    _server.setConnectionCallback(std::bind(&ChatServer::onConnection, this, placeholders::_1));

    // 给服务器注册用户读写时间回调
    _server.setMessageCallback(std::bind(&ChatServer::onMessage, this, placeholders::_1, placeholders::_2, placeholders::_3));

    // 设置服务器端的线程数量, 1个I/0线程，3个worker线程
    _server.setThreadNum(4);
}

// 开启事件循环
void ChatServer::start()
{
    _server.start();
}

// 专门处理用户的连接创建和断开  epoll   listenfd   accept
void ChatServer::onConnection(const TcpConnectionPtr& conn)
{
    if(conn->connected())
    {

    }
    else  // 客户端断开链接
    {
        // 断开链接之前把用户设置为下线状态，使用chatservice的_userModel
        ChatService::instance()->clientCloseException(conn);
        conn->shutdown();
    }
}


// 专门处理用户读写操作
// 如果有用户发送相关请求，就会调用对应的回调函数并执行onMessage()
void ChatServer::onMessage(const TcpConnectionPtr& conn, Buffer* buffer, Timestamp time)   // 连接    缓冲区     接收到数据的时间信息
{
    string buf = buffer->retrieveAllAsString();
    // 数据的反序列化
    json js = json::parse(buf);
    // 达到的目的：完全解耦网络模块的代码和业务模块的代码
    // 通过js["msgid"] 获取 -> 业务handler -> conn js time
    auto msgHandler = ChatService::instance()->getHandler(js["msgid"].get<int>());
    // 回调消息绑定好的事件处理器，来执行相应的业务处理
    msgHandler(conn, js, time);

    /*
        获取对应的处理器，并传入参数：
        如果js["msgid"]==LOGIN_MSG，就是登录业务，对应ChatService::login()函数
        如果js["msgid"]==REG_MSG，就是注册业务，对应ChatService::reg()函数
    */
}