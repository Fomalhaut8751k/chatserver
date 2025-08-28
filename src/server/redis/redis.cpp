#include "redis.hpp"
#include <iostream>
using namespace std;

Redis::Redis()
    :_publish_context(nullptr), _subscribe_context(nullptr)
{

}

Redis::~Redis()
{
    if(_publish_context != nullptr)
    {
        // 释放上下文的资源
        redisFree(_publish_context);
    }
    if(_subscribe_context != nullptr)
    {
        redisFree(_subscribe_context);
    }
}

// 连接redis服务器
bool Redis::connect()
{
    // 负责publish发布消息的上下文连接
    _publish_context = redisConnect("127.0.0.1", 6379);
    if(_publish_context == nullptr)
    {
        cerr << "connect redis failed!" << endl;
        return false;
    }

    redisReply* reply = (redisReply*)redisCommand(_publish_context, "AUTH %s", "123456");
    if (reply == nullptr) {
        cerr << "Authentication failed: null reply" << std::endl;
        redisFree(_publish_context);
        return -1;
    }

    // 负责subscribe发布消息的上下文连接
    _subscribe_context = redisConnect("127.0.0.1", 6379);
    if(_subscribe_context == nullptr)
    {
        cerr << "connect redis failed!" << endl;
        return false;
    }

    reply = (redisReply*)redisCommand(_subscribe_context, "AUTH %s", "123456");
    if (reply == nullptr) {
        cerr << "Authentication failed: null reply" << std::endl;
        redisFree(_subscribe_context);
        return -1;
    }

    // 在单独的线程中，监听通道上的事件，有消息给业务层上报
    thread t([&](){
        observer_channel_message();
    });
    t.detach();

    cerr << "connect redis success!" << endl;

    return true;
}

// 向redis指定的通道channel发布消息
bool Redis::publish(int channel, string message)
{
    // 发送一个命令，相当于在命令行(redis) publish 13 "helloworld"
    redisReply* reply = (redisReply*)redisCommand(this->_publish_context, "PUBLISH %d %s", channel, message.c_str());
    /* redisCommand
        先把要发送的命令缓存到本地，即调用redisAppendCommand()函数
        然后把命令发送的redis server上，即调用redisBufferWrite()函数
        再调用redisGetReply，以阻塞的方法等待命令的执行结果

        publish发送后不像subscribe会阻塞进行等待，一执行，就有回复，所以可以直接redisCommand
    */
    if(reply == nullptr)
    {
        cerr << "publish command failed!" << endl;
        return false;
    }
    freeReplyObject(reply);
    return true;
}

// 向redis指定的通道subscribe订阅消息
bool Redis::subscribe(int channel)
{
    // SUBSCRIBE命令本身会造成线程阻塞等待通道里面发生消息，这里只做订阅通道，不接受通道消息
    // 通道消息的接受专门在obserr_channel_message函数中的独立线程中进行
    // 只负责发送命令，不阻塞接受redis server响应消息，否则和notifyMsg线程抢占资源

    /* redisCommand
        先把要发送的命令缓存到本地，即调用redisAppendCommand()函数
        然后把命令发送的redis server上，即调用redisBufferWrite()函数
        再调用redisGetReply，以阻塞的方法等待命令的执行结果
        
        在Redis::subscribe函数下，没有直接使用redisCommand，而是只使用了redisAppendCommand以及redisBufferWrite()
        不调用redisGetReply阻塞等待结果
        redisGetReply的阻塞发生在observer_channel_message中
    */
    if(REDIS_ERR == redisAppendCommand(this->_subscribe_context, "SUBSCRIBE %d", channel))
    {
        cerr << "subscribe command failed!" << endl;
        return false;
    }
    // redisBUfferWrite可以循环发送缓冲区，直到缓冲区数据发送完毕(done被置为1)
    int done = 0;
    while(!done)
    {
        if(REDIS_ERR == redisBufferWrite(this->_subscribe_context, &done))
        {
            cerr << "subscribe command failed!" << endl;
            return false;
        }
    }
    return true;
}

// 向redis指定的通道unsubscribe取消订阅消息
bool Redis::unsubscribe(int channel)
{
    if(REDIS_ERR == redisAppendCommand(this->_subscribe_context, "UNSUBSCRIBE %d", channel))
    {
        cerr << "unsubscribe command failed!" << endl;
        return false;
    }
    // redisBUfferWrite可以循环发送缓冲区，直到缓冲区数据发送完毕(done被置为1)
    int done = 0;
    while(!done)
    {
        if(REDIS_ERR == redisBufferWrite(this->_subscribe_context, &done))
        {
            cerr << "unsubscribe command failed!" << endl;
            return false;
        }
    }
    return true;
}

// 在【独立线程】中接受订阅通道中的消息
void Redis::observer_channel_message()
{
    redisReply* reply = nullptr;
    // 以循环阻塞的方法等待上下文
    // 多个通道同时等待，如果有一个通道有消息，就会结束阻塞
    while(REDIS_OK == redisGetReply(this->_subscribe_context, (void **)&reply))
    {
        // 订阅收到的消息是一个带三元素的数组，有不为空的消息

        if(reply != nullptr && reply->element != nullptr && reply->element[2] != nullptr && reply->element[2]->str != nullptr)
        {
            // 回调函数，给业务层上报通道上发生的消息：通道号，消息内容
            _notify_message_handler(atoi(reply->element[1]->str), reply->element[2]->str);
        }
        freeReplyObject(reply);

        cerr << ">>>>>>>>>>>>> observer_channel_message quit <<<<<<<<<<<<<<<" << endl;
    }
}

// 初始化向业务层上报通道消息的回调函数
void Redis::init_notify_handler(function<void(int, string)> fn)
{
    this->_notify_message_handler = fn;  // 回调操作，收到订阅的消息，给service层上报
}