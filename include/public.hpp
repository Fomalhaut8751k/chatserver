#ifndef PUBLIC_H
#define PUBLIC_H

#include <string>

using namespace std;

/*
    server和client的公共文件
*/
enum EnMsgType
{
    LOGIN_MSG = 1,   // 登录消息            1
    LOGIN_MSG_ACK,   // 登陆响应消息        2

    REG_MSG,         // 注册消息            3
    REG_MSG_ACK,     // 注册响应消息        4
    
    ONE_CHAT_MSG,    // 聊天消息            5
    
    ADD_FRIEND_MSG,  // 添加好友消息        6
    ADD_FRIEND_ACK,  // 添加好友响应        7
    SHOW_FRIEND_ACK, // 展示好友列表        8

    CREATE_GROUP_MSG,// 创建群组            9
    CREATE_GROUP_ASK,// 创建或添加群组响应  10
    ADD_GROUP_MSG,   // 加入群组           11
    GROUP_CHAT_MSG,  // 群聊天             12
    CHECK_GROUP_MEM, // 查看该组成员       13
    CHECK_MY_GROUP,  // 查看加入的群聊     14

    LOGINOUT_MSG,    // 退出登陆          15
};


#endif