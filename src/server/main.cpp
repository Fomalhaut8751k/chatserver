#include "chatserver.hpp"
#include "chatservice.hpp"
#include <iostream>

#include <signal.h>

using namespace std;

// 处理服务器ctrl+c结束后，重置user的状态信息
void resetHandler(int)
{
    ChatService::instance()->reset();
    exit(0);
}

int main(int agrc, char** argv)
{
    signal(SIGINT, resetHandler);

    char* ip = argv[1];
    uint16_t port = atoi(argv[2]);

    EventLoop loop;  // epoll
    // InetAddress addr("127.0.0.1", 6000);
    InetAddress addr(ip, port);
    ChatServer server(&loop, addr, "ChatServer");

    server.start();  // listenfd epoll_ctl=>epoll
    loop.loop();  // epool_wait 阻塞方式等待新用户连接，或者已连接用户的读写事件等
    
    return 0;
}