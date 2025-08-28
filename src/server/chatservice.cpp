#include "chatservice.hpp"
#include "public.hpp"
#include "json.hpp"
#include "group.hpp"
#include <muduo/base/Logging.h>

#include <iostream>
#include <string>
#include <functional>
#include <vector>

using namespace muduo;
using namespace std;
using namespace placeholders;
using json = nlohmann::json;

// 获取单例对象的接口函数
ChatService* ChatService::instance()
{
    static ChatService service;
    return &service;
}

// 注册消息以及对应的Handler回调操作
ChatService::ChatService()
{
    _msgHandlerMap.insert({LOGIN_MSG, std::bind(&ChatService::login, this, _1, _2, _3)});
    _msgHandlerMap.insert({REG_MSG, std::bind(&ChatService::reg, this, _1, _2, _3)});
    _msgHandlerMap.insert({ONE_CHAT_MSG, std::bind(&ChatService::oneChat, this, _1, _2, _3)});
    _msgHandlerMap.insert({ADD_FRIEND_MSG, std::bind(&ChatService::addFriend, this, _1, _2, _3)});
    _msgHandlerMap.insert({SHOW_FRIEND_ACK, std::bind(&ChatService::showFriend, this, _1, _2, _3)});
    _msgHandlerMap.insert({CREATE_GROUP_MSG, std::bind(&ChatService::createGroup, this, _1, _2, _3)});
    _msgHandlerMap.insert({ADD_GROUP_MSG, std::bind(&ChatService::addGroup, this, _1, _2 ,_3)});
    _msgHandlerMap.insert({GROUP_CHAT_MSG, std::bind(&ChatService::groupChat, this, _1, _2, _3)});
    _msgHandlerMap.insert({CHECK_GROUP_MEM, std::bind(&ChatService::groupMemberShow, this, _1, _2, _3)});
    _msgHandlerMap.insert({CHECK_MY_GROUP, std::bind(&ChatService::groupShow, this, _1, _2, _3)});
    _msgHandlerMap.insert({LOGINOUT_MSG, std::bind(&ChatService::loginout, this, _1, _2, _3)});

    // // 连接redis服务器
    if(_redis.connect())
    {
        // 设置上报消息的回调
        _redis.init_notify_handler(std::bind(&ChatService::handleRedisSubscribeMessage, this, _1, _2));
    }
}


// 处理登录业务
void ChatService::login(const TcpConnectionPtr& conn, json& js, Timestamp)
{
    // {"msgid": 1, "id": 16, "password": "123456"}

    /*
        用户发来的json登陆请求，包含用户的id和用户的密码
        根据这个id，从数据库查找对应的用户信息，如用户名，密码，登陆状态，当前连接等
        如果找不到id对应的用户，或者找到了但密码不匹配，则登陆失败
        如果id密码都匹配，但是用户已经在线，也登陆失败
    */
    LOG_INFO << "do login service!!!";

    cerr << js << endl;

    int id = js["id"];
    string pwd = js["password"];

    // _userModel是chatservice中的一个成员变量，处理跟数据库相关的操作
    User user = _userModel.query(id); 
    if(user.getId() != -1 && user.getPwd() == pwd)
    {
        if(user.getState() == "online")
        {
            // 该用户已经登陆，不允许重复登陆
            json response;
            response["msgid"] = LOGIN_MSG_ACK;
            response["errno"] = 2;
            response["errmsg"] = "the account has already been logged in, please re-enter a new account";
            conn->send(response.dump());
        }
        // 登陆成功
        else
        {
            // 向存储在线用户的通信连接的map中添加
            {
                std::unique_lock<std::mutex> lock(_connMutex); 
                _userConnMap.insert({id, conn});
            }

            // 登陆成功，更新用户状态信息 state offline=>online
            _redis.subscribe(id);
           
            user.setState("online");
            _userModel.updateState(user);

            json response;
            response["msgid"] = LOGIN_MSG_ACK;
            response["errno"] = 0;
            response["id"] = user.getId();
            response["name"] = user.getName();
            
            conn->send(response.dump());

            // 如果登陆成功，就把离线过程收到的信息都显示出来
            vector<string> message_list = _offlineMessageModel.query(id);
            for(string& message: message_list)
            {
                // string -> json
                json jsbuf = json::parse(message); 
                // 把离线时接受到的消息逐一从服务器发送回来
                conn->send(jsbuf.dump());
            }
            _offlineMessageModel.erase(id);
        }
    }
    else
    {
        // 登陆失败
        json response;
        response["msgid"] = LOGIN_MSG_ACK;
        response["errno"] = 1;
        response["errmsg"] = "incorrect username or password";
        conn->send(response.dump());
    }
}


// 处理注册业务   name  password
void ChatService::reg(const TcpConnectionPtr& conn, json& js, Timestamp)
{
    // {"msgid": 3, "name": "yezhenhao", "password": "123456"}

    LOG_INFO << "do reg service!!!";

    string name = js["name"];
    string pwd = js["password"];

    User user;
    user.setName(name);
    user.setPwd(pwd);
    bool state = _userModel.insert(user);
    if(state)
    {
        // 注册成功
        json response;
        response["msgid"] = REG_MSG_ACK;
        response["errno"] = 0;
        response["id"] = user.getId();
        conn->send(response.dump());
    }
    // else
    // {
    //     // 注册失败
    //     json response;
    //     response["msgid"] = REG_MSG_ACK;
    //     response["errno"] = 1;
    //     conn->send(response.dump());
    // }
}

// 一对一聊天业务
void ChatService::oneChat(const TcpConnectionPtr& conn, json& js, Timestamp)
{
    // {"msgid": 5, "id": 19, "from": "shuoshuo", "to": 17, "msg": "wudongwei chulie!"}

    // 首先得是登录状态, 通过查找map中是否有对应的键值对
    int id = 0;
    // auto it = _userConnMap.begin();
    // while(it != _userConnMap.end())
    // {
    //     if((*it).second == conn)
    //     {
    //         id = (*it).first;
    //         break;
    //     }
    //     it++;
    // }

    // if(it == _userConnMap.end())
    // {
    //     // 该账号未登录
    //     json response;
    //     response["msgid"] = LOGIN_MSG_ACK;
    //     response["errno"] = 1;
    //     conn->send(response.dump());
    //     return;
    // }

    // json message;
    // message["msgid"] = ONE_CHAT_MSG;
    // message["from"] = js["from"];
    // message["id"] = js["id"];
    // message["msg"] = js["msg"];

    // 查找对应目标id的连接
    TcpConnectionPtr target_conn;

    {
        std::unique_lock<std::mutex> lock(mutex);
        // target_conn = _userConnMap[js["to"].get<int>()];  应该使用find，这样如果找不到会创建键值对
        auto it = _userConnMap.find(js["to"].get<int>());
        
        // 如果对方在线，就直接发送信息给他
        if(it != _userConnMap.end())
        {
            // 发送消息这里也要保证线程安全，否则可能出现准备发的时候对方下线的情况
            target_conn = it->second; 
            target_conn->send(js.dump());
            return;
        }

        User user = _userModel.query(js["to"].get<int>());  // 通过数据库去查询
        // 如果是在线的，说明不在当前服务器上登陆
        if(user.getState() == "online")
        {
            _redis.publish(js["to"].get<int>(), js.dump());
            return;
        }
    }

    // 说明对方不在线, 就把消息放到table：offlinemessage
    _offlineMessageModel.insert(js["to"].get<int>(), js.dump());

    return;
}

// 处理客户端异常退出
void ChatService::clientCloseException(const TcpConnectionPtr& conn)
{
    int id = 0;
    auto it = _userConnMap.begin();
    while(it != _userConnMap.end())
    {
        if((*it).second == conn)
        {
            id = (*it).first;
            break;
        }
        it++;
    }

    // 更新用户状态
    if(it != _userConnMap.end())
    {
        User user = _userModel.query(id); 
        user.setState("offline");
        _userModel.updateState(user);  
    }

    // 从map表删除用户的连接信息
    {
        std::unique_lock<std::mutex> lock(_connMutex);
        _userConnMap.erase(it);
    }  
}

// 服务器异常，业务重置方法
void ChatService::reset()
{
    // 把online的所有用户的登陆状态置为offline
    _userModel.resetState();
}


// 获取消息对应的处理器
MsgHandler ChatService::getHandler(int msgid)
{
    // 记录错误日志，msgid没有对应的时间处理回调
    auto it = _msgHandlerMap.find(msgid);
    if(it == _msgHandlerMap.end())
    {
        return [=](const TcpConnectionPtr& conn, json& js, Timestamp)->void{
            LOG_ERROR << "msgid: " << msgid << " can not find handler!";
        };
    }
    return _msgHandlerMap[msgid];
}

// 添加好友业务
bool ChatService::addFriend(const TcpConnectionPtr& conn, json& js, Timestamp)
{
    // {"msgid": 6, "id": 17, "from": "zhaozhendong", "friendid": 16}

    LOG_INFO << "do add friend service!!!";
    int userid = js["id"].get<int>();
    int friendid = js["friendid"].get<int>();

    // 判断friendid是否存在？
    User user = _userModel.query(friendid);  // 但是如果存在连密码都爆出来了
    json response;

    if(user.getId() != -1)  // 如果存在
    {
        if(_friendModel.insert(userid, friendid))
        {
            response["msgid"] = 7;
            response["errno"] = 0;
            response["id"] = friendid;
            response["msg"] = "add friend success!";
            conn->send(response.dump());

            return true;
        }
        else
        {
            response["msgid"] = 7;
            response["errno"] = 1;
            response["id"] = friendid;
            response["errmsg"] = "add friend failed! this ID is already your friend";
            conn->send(response.dump());

            return false;
        }
    }
    
    response["msgid"] = 7;
    response["errno"] = 1;
    response["errmsg"] = "add friend failed! this id is not exist!";
    conn->send(response.dump());

    return false;
}

// 查看好友列表
void ChatService::showFriend(const TcpConnectionPtr& conn, json& js, Timestamp)
{
    // {"msgid": 8, "id": 16}

    LOG_INFO << "do show friend service!!!";
    int userid = js["id"].get<int>();
    vector<string> my_friend_list = _friendModel.query(userid);
    json response;

    response["msgid"] = SHOW_FRIEND_ACK;

    int index = 1;
    for(string& friend_id_str: my_friend_list)
    {
        string friend_key_prefix = "friend_";
        response[friend_key_prefix + to_string(index)] = friend_id_str;
        index++;
    }

    response["msg"] = "the friend list has been loaded!";
    conn->send(response.dump());

    return;
}


// 创建群组业务
void ChatService::createGroup(const TcpConnectionPtr& conn, json& js, Timestamp)
{
    // {"msgid": 9, "id": 17, "name": "luqun", "desc": "luguanluguanluguanlulushijiandaole"}

    int userid = js["id"].get<int>();
    string group_name = js["name"];
    string group_desc = js["desc"];

    // 创建群组
    Group group(-1, group_name, group_desc);
    _groupModel.createGroup(group);
    int groupid = group.getId();

    // 加入群组并成为管理员
    _groupModel.addGroup(userid, groupid, "creator");

    json response;
    response["msgid"] = CREATE_GROUP_ASK;
    response["errno"] = 0;
    response["msg"] = "successfully created group!!!";

    conn->send(response.dump());

    return;
}


// 加入群组业务
void ChatService::addGroup(const TcpConnectionPtr& conn, json& js, Timestamp)
{
    // {"msgid": 11, "id": 17, "name": "wudongwei",  "groupid": 1}

    int userid = js["id"].get<int>();
    int groupid = js["groupid"].get<int>();

    string user_name = js["name"];

    // 加入群组并成为管理员
    _groupModel.addGroup(userid, groupid, "normal");

    json response;
    response["msgid"] = 10;
    response["errno"] = 0;
    response["msg"] = user_name + "joined the group chat";
}


// 查看群聊成员
void ChatService::groupMemberShow(const TcpConnectionPtr& conn, json& js, Timestamp)
{
    int groupid = js["groupid"].get<int>();
    vector<GroupUser> result = _groupModel.queryGroupUsers(groupid);
    json response;
    
    int member_number = result.size();
    response["number"] = member_number;
    int index = 1;

    for(GroupUser& group_member: result)
    {
        int member_id = group_member.getId();
        string member_name = group_member.getName();
        string member_state = group_member.getState();
        string member_role = group_member.getRole();

        string item = "";
        item += ("id: " + to_string(member_id));
        item += (", name: " + member_name);
        item += (", state: " + member_state);
        item += (", role: " + member_role);

        response["member(" + to_string(index) + ")"] = item;
        index++;
    }
    conn->send(response.dump());
}


// 群组聊天业务
void ChatService::groupChat(const TcpConnectionPtr& conn, json& js, Timestamp)
{
    // {"msgid": 12, "id": 19, "from": "shuoshuo", "togroup": 1, "msg": "wudongwei chulie!"}

    // 发送给指定群组的每一个人

    int userid = js["id"].get<int>();
    int groupid = js["togroup"].get<int>();
    string user_name = js["from"];
    string message = js["msg"];

    // 所有成员，也包括自己
    vector<GroupUser> group_ = _groupModel.queryGroupUsers(groupid);

    /*
        User user = _userModel.query(js["to"].get<int>());  // 通过数据库去查询
        / 如果是在线的，说明不在当前服务器上登陆
        if(user.getState() == "online")
        {
            _redis.publish(js["to"].get<int>(), js.dump());
            return;
        }
    */


    std::unique_lock<std::mutex> lock(_connMutex);
    {
        for(GroupUser& group_member : group_)
        {
            int group_member_id = group_member.getId();
            if(group_member_id == userid)
            {
                continue;
            }
            // 如果对方在线
            if(_userConnMap.find(group_member_id) != _userConnMap.end())
            {
                TcpConnectionPtr member_conn = _userConnMap[group_member_id];
                member_conn->send(js.dump());
            }
            // 如果对方不在本服务器上线
            else
            {
                User user = _userModel.query(group_member_id);  // 通过查询判断用户是否在线
                if(user.getState() == "online")  // 发现在其他服务器上登陆着
                {
                    _redis.publish(group_member_id, js.dump());
                }
                else
                {
                    _offlineMessageModel.insert(group_member_id, js.dump()); 
                }
               
            }  
        }
    }
    
}


// 查询加入的群聊
void ChatService::groupShow(const TcpConnectionPtr& conn, json& js, Timestamp)
{
    // {"msgid": 14, "id": 16}

    int userid = js["id"].get<int>();
    vector<Group> groups = _groupModel.queryGroups(userid);
    json response;
    response["number"] = groups.size();
    int index = 1;
    for(Group& group: groups)
    {
        int group_id = group.getId();
        string group_name = group.getName();
        string group_desc = group.getDesc();

        string item = "";
        item += (" group id: " + to_string(group_id));
        item += (", group name: " + group_name);
        item += (", group desc: " + group_desc);

        response["group(" + to_string(index) + ")"] = item;
        index++;
    } 
    conn->send(response.dump());
}


// 用户退出登录
void ChatService::loginout(const TcpConnectionPtr& conn, json& js, Timestamp)
{
    int userid = js["id"].get<int>();

    {
        lock_guard<mutex> lock(_connMutex);
        auto it = _userConnMap.find(userid);
        if(it != _userConnMap.end())
        {
            _userConnMap.erase(it);
        }
    }

    // 用户注销，相当于下线，在redis中取消订阅通道
    _redis.unsubscribe(userid);

    cerr << "userid: " << userid << " logout" << endl;

    // 更新用户状态信息
    User user(userid, "", "", "offline");
    _userModel.updateState(user);
}

// 从redis消息队列中获取订阅的消息
void ChatService::handleRedisSubscribeMessage(int userid, string msg)
{
    json js = json::parse(msg.c_str());

    unique_lock<mutex> lock(_connMutex);
    cerr << msg << endl;
    auto it = _userConnMap.find(userid);
    if(it != _userConnMap.end())
    {
        it->second->send(js.dump());
        return;
    }

    // 存储该用户的离线消息
    _offlineMessageModel.insert(userid, msg);
}
