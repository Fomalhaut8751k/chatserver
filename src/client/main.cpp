#include "json.hpp"
#include <iostream>
#include <thread>
#include <string>
#include <vector>
#include <chrono>
#include <ctime>
#include <mutex>
#include <condition_variable>

std::mutex mtx;
std::condition_variable cv;

#include <cstdlib>

using namespace std;
using json = nlohmann::json;

#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "group.hpp"
#include "user.hpp"
#include "public.hpp"

#include <limits>

void pauseProgram() {
    std::cout << "按回车键继续...";
    std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
}

unordered_map<string, int> status = {
    // 指示用户当前的聊天状态(不在聊天，某个好友聊天，某个群聊聊天)
    make_pair("friend", -1),
    make_pair("group", -1)
};


// 记录当前系统登陆的用户信息
User g_currentUser;
// 好友列表
unordered_map<int, User> friend_list;
// 群聊列表
unordered_map<int, Group> group_list;

// 记录用户接收到的好友消息
unordered_map<int, vector<json>> friendMessageSet;  // id , message
// 记录用户接收到的群聊消息
unordered_map<int, vector<json>> groupMessageSet;  // groupid , message

// 显示当前登陆成功用户的基本信息
void showCurrentUserData();

// 接受线程
void writeTaskHandler(int clientfd);
// 获取系统时间（聊天信息需要添加时间信息）
string getCurrentTime();
// 主聊天页面程序
void mainMenu(int id, string name, bool& user_online, int clientfd);
// 好友页面程序
void friendMenu(int id, string name, bool& show_friend, int clientfd);
// 群聊页面程序
void groupMenu(int id, string name, bool& chat_friend, int clientfd);
// 从字符串中获取好友信息
User getMsgFromString(string msg_str);
// 从字符串中获取群组信息
Group getMsgFromGroupString(string msg_str);
// 单个朋友聊天页面
void chatMenu(User user, string name, int id, int clientfd, bool& chat_friend);
// 群组聊天页面
void groupChatMenu(Group group, string name, int id, int clientfd, bool& chat_friend);
// 单人聊天接收线程
void readTaskHandler(int clientfd);
// 多人聊天接受线程
void readGroupTaskHandler(int clientfd, int id);

// 聊天消息和响应消息处理线程
void chatAndResponseHandler(int clientfd, int id);


int main(int argc, char** argv)
{
    if(argc < 3)
    {
        cerr << "command invaild!   example: ./ChatClient 127.0.0.1 6000" << endl;
        exit(-1);
    }

    // 解析通过命令行参数传递的ip和port
    char* ip = argv[1];
    uint16_t port = atoi(argv[2]);

    // 创建client端的socket
    int clientfd = socket(AF_INET, SOCK_STREAM, 0);
    if(-1 == clientfd)
    {
        cerr << "socket create error" << endl;
        exit(-1);
    }

    // 填写client需要连接的server信息ip+port
    sockaddr_in server;
    memset(&server, 0, sizeof(sockaddr_in));

    server.sin_family = AF_INET;
    server.sin_port = htons(port);
    server.sin_addr.s_addr = inet_addr(ip);

    // client和server进行连接
    if(-1 == connect(clientfd, (sockaddr*)& server, sizeof(sockaddr_in)))
    {
        perror("connect failed"); // 这会显示具体的系统错误信息
        cerr << "Error code: " << errno << endl;
        close(clientfd);
        exit(-1);
    }

    for(;;)
    {
        // 显示首页菜单 登陆，注册，退出
        cout << "==============================" << endl;
        cout << "[1]. login" << endl;
        cout << "[2]. register" << endl;
        cout << "[3]. quit" << endl;
        cout << "==> choice: ";
        int choice = 0;
        cin >> choice;
        cin.get();  // 读掉缓冲区残留的回车

        switch(choice)
        {
            case 1: // login业务
            {
                int id = 0;
                char pwd[50] = {0};
                cout << "userid: ";
                cin >> id;
                cin.get();  // 读掉缓冲区残留的回车
                cout << "userpassword: ";
                cin.getline(pwd, 50);

                json js;
                // {"msgid": 1, "id": 16, "password": "123456"}
                js["msgid"] = 1;
                js["id"] = id;
                js["password"] = pwd;
                string request = js.dump();

                cerr << js.dump() << endl;

                // 发送给客户端处理登陆业务
                int len = send(clientfd, request.c_str(), strlen(request.c_str()) + 1, 0);
                if(len == -1)
                {
                    cerr << "send login msg error: " << request << endl; 
                }
                else  // 无论登录成功与否，服务器都会想客户端发送消息send()
                {
                    char buffer[1024] = {0};
                    len = recv(clientfd, buffer, 1024, 0);
                    if(len == -1)
                    {
                        cerr << "recv login msg error" << endl;
                    }
                    else
                    {
                        json js = json::parse(buffer);
                        if(js["errno"].get<int>() == 1)  // 用户名密码错误
                        {
                            cerr << "incorrect username or password" << endl;
                        }
                        else if(js["errno"].get<int>() == 2)  // 账号已经在线
                        {
                            cerr << "the account has already been logged in, please re-enter a new account" << endl;
                        }
                        else
                        {
                            cerr << "( " << js["name"] << ") login success!!!" << endl;

                            pauseProgram();
                            system("clear");

                            // 记录当前有用户的id和name
                            g_currentUser.setId(js["id"].get<int>());
                            g_currentUser.setName(js["name"]);

                            // 登陆状态
                            bool user_online = true;

                            std::thread readTask(chatAndResponseHandler, clientfd,id);
                            readTask.detach();

                            while(user_online)
                            {
                                mainMenu(g_currentUser.getId(), g_currentUser.getName(), user_online, clientfd);
                            }
                        }

                    }
                }

            }break;
            case 2:  // register业务
            {
                char name[50] = {0};
                char pwd[50] = {0};
                cout << "username: ";
                cin.getline(name, 50);
                cout << "userpassword: ";
                cin.getline(pwd, 50);

                json js;
                // {"msgid": 3, "name": "van", "password": "123456"}
                js["msgid"] = REG_MSG;
                js["name"] = name;
                js["password"] = pwd;
                string request = js.dump();

                cerr << request << endl;

                int len = send(clientfd, request.c_str(), strlen(request.c_str()) + 1, 0);
                if(len == -1)
                {
                    cerr << "send reg msg error: " << request << endl;
                }
                else
                {
                    char buffer[1024] = {0};
                    len = recv(clientfd, buffer, 1024, 0);
                    if(-1 == len)
                    {
                        cerr << "recv reg response error" << endl;
                    }
                    else
                    {
                        json js = json::parse(buffer);
                        if(js["errno"].get<int>() == 1)
                        {
                            cerr << "register failed!!!" << endl;
                        }
                        else
                        {
                            cerr << "register success!!!, your id is [" << js["id"].get<int>() << "]" << endl; 
                        }
                    }
                }
            }break;
            case 3:
            {
                close(clientfd);
                exit(0);
            }
            default:
                cerr << "invalid input!" << endl;
                break;
        }

        pauseProgram();
        system("clear");
    }

    return 0;

}


// 主聊天页面程序
void mainMenu(int id, string name, bool& user_online, int clientfd)
{
    cerr << "==============================" << endl;
    cerr << "Hello, " << name << " (" << id << ")" << endl;
    cerr << "==============================" << endl;
    cerr << "[1]. show friend" << endl;
    cerr << "[2]. show group" << endl;
    cerr << "[3]. add friend" << endl;
    cerr << "[4]. add group" << endl;
    cerr << "[5]. create group" << endl;
    cerr << "[6]. exit" << endl;
    cerr << "==> choice: ";

    int choice = 0;
    cin >> choice;
    cin.get();  // 读掉缓冲区残留的回车

    switch(choice)
    {
        case 1:  // 查看好友
        {
            system("clear");
            bool show_friend = true;
            while(show_friend)
            {
                friendMenu(id, name, show_friend, clientfd);
                pauseProgram();
                system("clear"); 
            }
            
        }break;
        case 2:  // 查看群组
        {
            // {"msgid": 14, "id": 16}
            system("clear");
            bool show_group = true;
            while(show_group)
            {
                groupMenu(id, name, show_group, clientfd);
                pauseProgram();
                system("clear"); 
            }

        }break;
        case 3:  // 添加好友
        {
            // {"msgid": 6, "id": 17, "from": "van", "friendid": 16}
            cerr << "> please enter the ID of the friend you want to add: " ;
            int friendid = 0;
            cin >> friendid;
            cin.get();  // 读掉缓冲区残留的回车

            json request;
            request["msgid"] = 6;
            request["id"] = id;
            request["from"] = name;
            request["friendid"] = friendid;

            string request_str = request.dump();

            cerr << request_str << endl;

            int len = send(clientfd, request_str.c_str(), strlen(request_str.c_str()) + 1, 0);

            // if(len >= 1)
            // {
            //     cerr << "added friend successfully!!!" << endl;
            //     pauseProgram();
            //     system("clear"); 
            // }

            char buffer[1024] = {0};
            len = recv(clientfd, buffer, 1024, 0);
            if(len > 1 && json::parse(buffer)["errno"] == 0)
            {
                cerr << "added friend successfully!!!" << endl;
            }
            else
            {
                cerr << "added friend failed!!!" << endl;
            }

        }break;
        case 4:  // 添加群组
        {
            // {"msgid": 11, "id": 17, "name": "van",  "groupid": 2}
            cerr << "> please enter the ID of the group you want to add: " ;
            int groupid = 0;
            cin >> groupid;
            cin.get();  // 读掉缓冲区残留的回车

            json request;
            request["msgid"] = 11;
            request["id"] = id;
            request["name"] = name;
            request["groupid"] = groupid;

            string request_str = request.dump();
            cerr << request_str << endl;

            int len = send(clientfd, request_str.c_str(), strlen(request_str.c_str()) + 1, 0);

        }break;
        case 5:  // 加入群聊
        {
            // {"msgid": 9, "id": 16, "name": "group1", "desc": "this is a group for..."}
            string groupname = "";
            cerr << "please enter the name of the group you want to create: ";
            cin >> groupname;
            cin.get();

            string groupdesc = "";
            cerr << "\nplease enter the desc of ther group you want to create: ";
            cin >> groupdesc;
            cin.get();

            json js;
            js["msgid"] = 9;
            js["id"] = id;
            js["name"] = groupname;
            js["desc"] = groupdesc;

            string request_str = js.dump();

            int len = send(clientfd, request_str.c_str(), strlen(request_str.c_str()) + 1, 0);

        }break;
        default:  // 退出
        {
            json js;
            js["msgid"] = 15;
            js["id"] = id;
            

            // 把所有的离线消息都转发到offlinemessage中
            /*
                // 记录用户接收到的好友消息
                unordered_map<int, vector<json>> friendMessageSet;  // id , message
                // 记录用户接收到的群聊消息
                unordered_map<int, vector<json>> groupMessageSet;  // groupid , message
            */
            string msg_friend_set = "";
            string msg_group_set = "";

            // 读取好友消息
            for(pair<int, vector<json>> item: friendMessageSet)
            {
                // 某个好友发送的非窗口消息集合
                for(json js_msg: item.second)
                {
                    msg_friend_set += js_msg.dump();
                }
            }
            /*
                "{\"from\":\"wudongwei\",\"id\":17,\"msg\":\"握中有悬璧\",\"msgid\":5,\"to\":16}{\"from\":\"wudongwei\",
                \"id\":17,\"msg\":\"本自荆山璆\",\"msgid\":5,\"to\":16}{\"from\":\"wudongwei\",\"id\":17,\"msg\":\"惟彼太公望\",\"msgid\":"...
            */

            // 读取群聊消息
            for(pair<int, vector<json>> item: groupMessageSet)
            {
                // 某个群聊发送的非窗口消息集合
                for(json js_msg: item.second)
                {
                    msg_group_set += js_msg.dump();
                }
            }

            js["msgfriendset"] = msg_friend_set;
            js["msggroupset"] = msg_group_set;

            string request = js.dump();

            int len = send(clientfd, request.c_str(), strlen(request.c_str()) + 1, 0); 

            // 清空好友列表，群聊列表，非窗口消息，以防止当期客户端更换账号登录时出错
            friend_list.clear();
            group_list.clear();
            friendMessageSet.clear();
            groupMessageSet.clear();

            user_online = false;
            return;
        }
    }
    
}

// 好友页面程序
void friendMenu(int id, string name, bool& show_friend, int clientfd)
{
    cerr << "==============================" << endl;
    cerr << "Hello, " << name << " (" << id << ")" << endl;
    cerr << "==============================" << endl;

    json js;
    // {"msgid": 8, "id": 16}
    js["msgid"] = 8;
    js["id"] = id;

    string request = js.dump();

    // cerr << request << endl;
    friend_list.clear();

    int len = send(clientfd, request.c_str(), strlen(request.c_str()) + 1, 0);
    if(len == -1)
    {
        cerr << "send show friends msg error: " << request << endl;
    }
    else  // 如果发送成功
    {
        {
            std::unique_lock<std::mutex> lck(mtx);
            // char buffer[1024] = {0};
            // len = recv(clientfd, buffer, 1024, 0);
            cv.wait(lck);  // 等待，先阻塞，等消息接收线程处理完毕   
        }

        cerr << "------------------------------" << endl;
        cerr << "[1]. chat with friend" << endl;
        cerr << "[2]. update friend state" << endl;
        cerr << "[3]. exit" << endl;
        cerr << "==> choice: ";

        int choice = 0;
        cin >> choice;
        cin.get();  // 读掉缓冲区残留的回车

        switch(choice)
        {
            case 1:
            {
                int friendid = -1;
                cerr << "==> chat with friend whose id is: " << endl;
                cin >> friendid;
                cin.get();  // 读掉缓冲区残留的回车

                if(friend_list.find(friendid) == friend_list.end())  // 退出操作
                {
                    cerr << "this id of friend is not exist" << endl;
                    return;
                }

                bool chat_friend = true;
                User user = friend_list[friendid];
                
                // 设置用户当前打开的是哪个好友的聊天页面
                status["friend"] = friendid;

                system("clear"); 
                cerr << "==============================" << endl;
                cerr << "Hello, " << name << " (" << id << ")" << endl;
                cerr << "==============================" << endl;

                cerr << "@ " << user.getName() << " " << user.getId() << ":" << endl;
                
                // 把离线消息打印出来：
                
                json js_for_offlinemessage;
                js_for_offlinemessage["msgid"] = OFFLINE_MSG_GET;
                js_for_offlinemessage["id"] = id;
                js_for_offlinemessage["friendid"] = friendid;
                js_for_offlinemessage["groupid"] = -1;

                // 向服务器申请访问某个好友对应的离线消息
                int len = send(clientfd, js_for_offlinemessage.dump().c_str(), strlen(js_for_offlinemessage.dump().c_str())+1, 0);
               
                {
                    std::unique_lock<std::mutex> lck(mtx);
                    cv.wait(lck);  // 等待，先阻塞，等消息接收线程处理完毕   
                }

                // 把非窗口消息打印出来：
                for(json& js: friendMessageSet[friendid])
                {
                    cerr << "> ";
                    cerr << "[" << js["from"] << "]: " << js["msg"] << endl;    
                }
                // 清理打印出来的消息
                friendMessageSet[friendid].clear();

                // std::thread readTask(readTaskHandler, clientfd);

                while(chat_friend)
                {          
                    chatMenu(user, name, id, clientfd, chat_friend);
                }

                status["friend"] = -1;
                
                system("clear"); 

            }break;
            case 2:
            {
                
            }break;
            case 3:
            {
                show_friend = false;
                return;
            }
        }
        
    }

}

// 群聊页面程序
void groupMenu(int id, string name, bool& show_group, int clientfd)
{
    cerr << "==============================" << endl;
    cerr << "Hello, " << name << " (" << id << ")" << endl;
    cerr << "==============================" << endl;

    json request;
    request["msgid"] = 14;
    request["id"] = id;

    

    string request_str = request.dump();
    int len = send(clientfd, request_str.c_str(), strlen(request_str.c_str()) + 1, 0);

    if(len < 1)
    {
        cerr << "show group failed!!! unknown error" << endl;
        pauseProgram();
        system("clear");
        return;
    }
    // 如果发送成功
    else
    {
        {
            std::unique_lock<std::mutex> lck(mtx);
            // char buffer[1024] = {0};
            // len = recv(clientfd, buffer, 1024, 0);
            cv.wait(lck);  // 等待，先阻塞，等消息接收线程处理完毕   
        }

        cerr << "------------------------------" << endl;
        cerr << "[1]. chat with group" << endl;
        cerr << "[2]. update group state" << endl;
        cerr << "[3]. exit" << endl;
        cerr << "==> choice: ";

        int choice = 0;
        cin >> choice;
        cin.get();  // 读掉缓冲区残留的回车

        switch(choice)
        {
            case 1:
            {
                int groupid = -1;
                cerr << "==> chat with group whose id is: " << endl;
                cin >> groupid;
                cin.get();  // 读掉缓冲区残留的回车

                if(group_list.find(groupid) == group_list.end())  // 退出操作
                {
                    cerr << "this id of group is not exist" << endl;
                    return;
                }

                bool chat_group = true;
                Group group = group_list[groupid];
                // groupChatMenu(Group group, string name, int id, int clientfd, bool& chat_friend)

                system("clear"); 
                cerr << "==============================" << endl;
                cerr << "Hello, " << name << " (" << id << ")" << endl;
                cerr << "==============================" << endl;

                json js;
                js["msgid"] = 13;
                js["id"] = id;
                js["groupid"] = group.getId();

                // 查看群组成员
                int len = send(clientfd, js.dump().c_str(), strlen(js.dump().c_str()) + 1, 0);

                {
                    std::unique_lock<std::mutex> lck(mtx);
                    cv.wait(lck);  // 等待，先阻塞，等消息接收线程处理完毕   
                }

                cerr << "@ " << group.getName() << " " << group.getId() << ":" << endl;
                // 把离线消息打印出来：
                
                json js_for_offlinemessage;
                js_for_offlinemessage["msgid"] = OFFLINE_MSG_GET;
                js_for_offlinemessage["id"] = id;
                js_for_offlinemessage["friendid"] = -1;
                js_for_offlinemessage["groupid"] = group.getId();

                // 向服务器申请访问某个群聊对应的离线消息
                len = send(clientfd, js_for_offlinemessage.dump().c_str(), strlen(js_for_offlinemessage.dump().c_str())+1, 0);
               
                {
                    std::unique_lock<std::mutex> lck(mtx);
                    cv.wait(lck);  // 等待，先阻塞，等消息接收线程处理完毕   
                }

                // 把非窗口消息打印出来：
                for(json& js_e: groupMessageSet[group.getId()])
                {
                    cerr << "> ";
                    cerr << "[" << js_e["from"] << "]: " << js_e["msg"] << endl;    
                }
                // 清楚打印后的群聊消息
                groupMessageSet[group.getId()].clear();

                // std::thread readTask(readGroupTaskHandler, clientfd, id);

                status["group"] = group.getId();

                while(chat_group)
                {
                    groupChatMenu(group, name, id, clientfd, chat_group);
                }

                // {"msgid": 12, "id": 17, "from": "van", "togroup": 2, "msg": "I am an artist"}
                // {"msgid": 5, "id": 19, "from": "bili", "to": 17, "msg": "wushuangdahuanggua"}
                system("clear"); 

                status["group"] = -1;

                // json js_exit;
                // js_exit["msgid"] = 5;
                // js_exit["id"] = id;
                // js_exit["from"] = "";
                // js_exit["to"] = id;
                // js_exit["msg"] = "exit()";

                // string request_exit = js_exit.dump();
                // len = send(clientfd, request_exit.c_str(), strlen(request_exit.c_str()) + 1, 0);

                // readTask.join();

            }break;
            case 2:
            {
                
            }break;
            case 3:
            {
                show_group = false;
                return;
            }
        }

    }

    

}

// 单个朋友聊天页面
void chatMenu(User user, string name, int id, int clientfd, bool& chat_friend)
{
    char msg[1024] = {0};
    cerr << "> ";
    cin.getline(msg, 1024);

    if(strcmp(msg, "exit()") == 0)
    {
        chat_friend = false;
        return;
    }

    json message;
    // {"msgid": 5, "id": 19, "from": "van", "to": 17, "msg": "deep dark fantasies"}
    message["msgid"] = 5;
    message["id"] = id;
    message["from"] = name;
    message["to"] = user.getId();
    message["msg"] = msg;

    string request = message.dump();
    int len = send(clientfd, request.c_str(), strlen(request.c_str()) + 1, 0);
    
    // char buffer[1024] = {0};
    // len = recv(clientfd, buffer, 1024, 0);
    // cerr << buffer << endl;

}

// 群组聊天页面
void groupChatMenu(Group group, string name, int id, int clientfd, bool& chat_friend)
{
    // {"msgid": 13, "id": 17, "groupid": 1}
    char msg[1024] = {0};
    cerr << "> ";
    cin.getline(msg, 1024);

    if(strcmp(msg, "exit()") == 0)
    {
        chat_friend = false;
        return;
    }

    json message;
    // {"msgid": 12, "id": 17, "from": "van", "togroup": 2, "msg": "i am hired for people"}
    message["msgid"] = 12;
    message["id"] = id;
    message["from"] = name;
    message["togroup"] = group.getId();
    message["msg"] = msg;

    string request = message.dump();
    int len = send(clientfd, request.c_str(), strlen(request.c_str()) + 1, 0);

    
}

// 从字符串中获取好友信息
User getMsgFromString(string msg_str)
{
    // " id: 17 name: van state: offline"
    int id_start_index = msg_str.find("id: ") + 4;
    int id_end_index = msg_str.find(" name: ") - 1;

    int name_start_index = msg_str.find("name: ") + 6;
    int name_end_index = msg_str.find(" state: ") - 1;

    int state_start_index = msg_str.find("state: ") + 7;
    int state_end_index = msg_str.length() - 1;

    User user;
    user.setId(atoi(msg_str.substr(id_start_index, id_end_index-id_start_index+1).c_str()));
    user.setName(msg_str.substr(name_start_index, name_end_index-name_start_index+1));
    user.setState(msg_str.substr(state_start_index, state_end_index-state_start_index+1));


    return user;
}

// 从字符串中获取群组信息
Group getMsgFromGroupString(string msg_str)
{
    // " group id: 2, group name: group2, group desc: 111"
    int group_id_start_index = msg_str.find("group id: ") + 10;
    int group_id_end_index = msg_str.find("group name: ") - 1;

    int group_name_start_index = msg_str.find("group name: ") + 12;
    int group_name_end_index = msg_str.find(" group desc: ") - 2;

    int group_desc_start_index = msg_str.find("group desc:") + 12;
    int group_desc_end_index = msg_str.length() - 2;

    Group group;
    group.setId(atoi(msg_str.substr(group_id_start_index,  group_id_end_index-group_id_start_index+1).c_str()));
    group.setName(msg_str.substr(group_name_start_index, group_name_end_index-group_name_start_index+1));
    group.setDesc(msg_str.substr(group_desc_start_index, group_desc_end_index-group_desc_start_index+1));

    return group;

}

// 单人聊天接收线程
void readTaskHandler(int clientfd)
{
    while(1)
    {
        char buffer[1024] = {0};
        // {"msgid": 5, "id": 19, "from": "van", "to": 17, "msg": "111"}
        int len = recv(clientfd, buffer, 1024, 0);
        if(len == -1 || len == 0)
        {
            close(clientfd);
            exit(-1);
        }

        json js = json::parse(buffer);
        if(js["msg"] == "exit()") 
        {
            break;
        }
        if(ONE_CHAT_MSG == js["msgid"].get<int>())
        {
            // {"msgid": 5, "id": 19, "from": "苻坚", "to": 17, "msg": "昔朕以龙骧建业，未尝轻以授人，卿其勉之"}
            cerr << "[" << js["from"] << "]: " << js["msg"] << endl; 
            cerr << "> ";
        }
    }
}

// 多人聊天接受线程
void readGroupTaskHandler(int clientfd, int id)
{
    while(1)
    {
        char buffer[1024] = {0};
        
        int len = recv(clientfd, buffer, 1024, 0);
        if(len == -1 || len == 0)
        {
            close(clientfd);
            exit(-1);
        }

        json js = json::parse(buffer);

        if(js["msg"] == "exit()" && js["id"] == id) 
        {
            break;
        }
        if(GROUP_CHAT_MSG == js["msgid"].get<int>())
        {
            // {"msgid": 12, "id": 17, "from": "van", "togroup": 2, "msg": "123"}
            cerr << "[" << js["from"] << "]: " << js["msg"] << endl; 
            cerr << "> ";
        }
    }
}

// 聊天消息和响应消息处理线程
void chatAndResponseHandler(int clientfd, int id)
{
    for(;;)
    {
        char buffer[1024] = {0};
        
        int len = recv(clientfd, buffer, 1024, 0);
        if(len == -1 || len == 0)
        {
            close(clientfd);
            exit(-1);
        }

        string buffer_str = buffer;
        int right_pos = -1;

        // 可能包含了多个json消息
        while(1)
        {
            int new_right_pos = buffer_str.find('}', right_pos + 1);  // 查找'{'的位置
            if(new_right_pos == string::npos)  // 如果没找到，说明照完了
            {
                break;
            }
            int new_left_pos = right_pos + 1;
            // 如果下一个{在当前的}的左边，说明出现了{}的嵌套
            int new_left_pos_check = new_left_pos;
            while(buffer_str.find('{', new_left_pos_check + 1) < new_right_pos)
            {
                new_right_pos = buffer_str.find('}', new_right_pos + 1);
                new_left_pos_check = buffer_str.find('{', new_left_pos_check + 1);
            }
            string buffer_substr = buffer_str.substr(new_left_pos, new_right_pos - new_left_pos + 1);
            
            json js = json::parse(buffer_substr);
            int msgid = js["msgid"];
            // 如果是好友群聊消息
            if(msgid == 5)  // 好友聊天消息
            {
                int friend_id = js["id"];  // 好友id
                // 如果正好处在该好友的聊天页面,直接打印消息
                if(status["friend"] == friend_id && status["group"] == -1)
                {
                    cerr << "[" << js["from"] << "]: " << js["msg"] << endl; 
                    cerr << "> ";
                }
                else  // 不是就放到消息队列中
                {
                    std::unique_lock<std::mutex> lck(mtx);
                    friendMessageSet[friend_id].push_back(js);
                }       
            }
            else if(msgid == 12)  // 群组聊天消息
            {
                int group_id = js["togroup"];  // 群聊id
                // 如好正好处在该群聊的聊天页面，直接打印消息
                if(status["friend"] == -1 && status["group"] == group_id)
                {
                    cerr << "[" << js["from"] << "]: " << js["msg"] << endl; 
                    cerr << "> ";
                }
                else  // 不是就放到消息队列中
                {
                    std::unique_lock<std::mutex> lck(mtx); 
                    groupMessageSet[group_id].push_back(js);
                }
            }

            // 如果是业务反馈消息
            else
            {
                // 添加好友响应  msgid = 7
                if(js["msgid"] == ADD_FRIEND_ACK)
                {
                    if(js["errno"] == 0)
                    {
                        cerr << "added friend successfully!!!" << endl;
                    }
                    else
                    {
                        cerr << "added friend failed!!!" << endl;
                    }
                }
                // 查看好友列表响应 msgid = 8
                else if(js["msgid"] == SHOW_FRIEND_ACK)
                {
                    // 这个消息中包含所有的好友信息
                    int index = 0;
                    // 打印好友信息
                    for (auto& [key, value] : js.items()) 
                    {   
                        if(key.substr(0, 6) == "friend")
                        {
                            cerr << value << endl;

                            User user = getMsgFromString(value.dump());

                            friend_list.insert({user.getId(), user});
                            
                            index++;
                        }
                    }
                    cerr << index << " people in total." << endl;
                    cv.notify_all(); // 唤醒主线程
                }
                // 创建或添加群聊业务响应 msgid = 10
                else if(js["msgid"] == CREATE_GROUP_ASK)
                {
                    if(json::parse(buffer_substr)["errno"] == 0)
                    {
                        cerr << "successfully!!!" << endl;
                    }
                    else
                    {
                        cerr << "failed!!!" << endl;
                    }
                }

                // 查看所有群聊响应 msgid = 14
                else if(js["msgid"] == CHECK_MY_GROUP)
                {
                    // 这个消息中包含加入的所有的群聊
                    int index = 0;

                    int length = json::parse(buffer_substr).size();

                    // 打印所有群聊
                    for (auto it: js) 
                    {   
                        index++;
                        if(index == length)
                        {
                            break;
                        }
                        // msgid不用打印出来
                        if(it.dump() == "14")
                        {
                            continue;
                        }
                        Group group = getMsgFromGroupString(it.dump());
                        group_list.insert({group.getId(), group});
                        cerr << it << endl;
                    }
                    cerr << js["number"] << " group in total." << endl;
                    cv.notify_all(); // 唤醒主线程
                }
                // 显示群聊内部群友形信息 msgid = 13
                else if(js["msgid"] == CHECK_GROUP_MEM)
                {                  
                    int length = json::parse(buffer_substr).size();

                    int index = 0;

                    cerr << "group member: " << endl;

                    for(auto it : json::parse(buffer_substr))
                    {   
                        index++;
                        if(index == length)
                        {
                            break;
                        }
                        if(it.dump() == "13")
                        {
                            continue;
                        }
                        cerr << it << endl;
                    }
                    cerr << json::parse(buffer)["number"] << " member in total" << endl;
                    cv.notify_all(); // 唤醒主线程
                }
                // 接受服务器返回的离线消息 msgid = 17
                else if(js["msgid"] == OFFLINE_MSG_GET)
                {
                    vector<string> off_msg_set = js["msgset"];
                    for(string& msg: off_msg_set)
                    {
                        // cerr << msg << endl;
                        json js_off_msg = json::parse(msg);
                        cerr << "> ";
                        cerr << "[" << js_off_msg["from"] << "]: " << js_off_msg["msg"] << endl;            
                    }
                    cv.notify_all(); // 唤醒主线程
                }
            }
           
            right_pos = new_right_pos;
        }
    }
}