#ifndef CHATSERVICE_H
#define CHATSERVICE_H

#include "redis.hpp"
#include "json.hpp"
#include "usermodel.hpp"
#include "offlinemessagemodel.hpp"
#include "friendmodel.hpp"
#include "groupmodel.hpp"

#include <muduo/net/TcpConnection.h>
#include <unordered_map>
#include <functional>
#include <memory>
#include <mutex>

using namespace muduo;
using namespace muduo::net;

// 表示处理消息的事件回调方法类型
using json = nlohmann::json;
using MsgHandler = std::function<void(const TcpConnectionPtr& conn, json& js, Timestamp)>;


// 聊天服务器业务类
class ChatService
{
public:
    // 获取单例对象的接口函数
    static ChatService* instance();

    // 处理登录业务
    void login(const TcpConnectionPtr& conn, json& js, Timestamp);

    // 处理注册业务
    void reg(const TcpConnectionPtr& conn, json& js, Timestamp);

    // 一对一聊天业务
    void oneChat(const TcpConnectionPtr& conn, json& js, Timestamp);

    // 处理客户端异常退出
    void clientCloseException(const TcpConnectionPtr& conn);

    // 添加好友业务
    bool addFriend(const TcpConnectionPtr& conn, json& js, Timestamp);

    // 查看好友列表
    void showFriend(const TcpConnectionPtr& conn, json& js, Timestamp);

    // 创建群组业务
    void createGroup(const TcpConnectionPtr& conn, json& js, Timestamp);

    // 加入群组业务
    void addGroup(const TcpConnectionPtr& conn, json& js, Timestamp);

    // 群组聊天业务
    void groupChat(const TcpConnectionPtr& conn, json& js, Timestamp);

    // 查看群聊成员
    void groupMemberShow(const TcpConnectionPtr& conn, json& js, Timestamp);

    // 查询加入的群聊
    void groupShow(const TcpConnectionPtr& conn, json& js, Timestamp);

    // 用户退出登录
    void loginout(const TcpConnectionPtr& conn, json& js, Timestamp);

    // 服务器异常，业务重置方法
    void reset();

    // 从redis消息队列中获取订阅消息
    void handleRedisSubscribeMessage(int userid, string msg);

    // 获取消息对应的处理器
    MsgHandler getHandler(int msgid);

private:
    ChatService();  // 构造函数私有化

    // 存储消息id和其对应的业务处理方法
    /*
        _msgHandlerMap.insert({LOGIN_MSG, std::bind(&ChatService::login, this, _1, _2, _3)});
    _    msgHandlerMap.insert({REG_MSG, std::bind(&ChatService::reg, this, _1, _2, _3)});
    */
    unordered_map<int, MsgHandler> _msgHandlerMap;

    // 存储在线用户的通信连接
    unordered_map<int, TcpConnectionPtr> _userConnMap; 

    // 数据操作类对象
    UserModel _userModel;

    // 离线信息操作类对象
    OfflineMessageModel _offlineMessageModel;

    // 好友信息操作类对象
    FriendModel _friendModel;

    // 群组相关操作类对象
    GroupModel _groupModel;

    // 控制线程的互斥锁
    std::mutex _connMutex;

    // redis操作对象
    Redis _redis;
};

#endif