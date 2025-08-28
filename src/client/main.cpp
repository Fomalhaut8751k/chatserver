#include "json.hpp"
#include <iostream>
#include <thread>
#include <string>
#include <vector>
#include <chrono>
#include <ctime>

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

// 记录当前系统登陆的用户信息
User g_currentUser;
// 记录当前登陆用户的好友列表信息
vector<User> g_currentUserFriendList;
// 记录当前登陆用户的群组列表信息
vector<Group> g_currentUserGroupList;
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

                int len = send(clientfd, request.c_str(), strlen(request.c_str()) + 1, 0);
                if(len == -1)
                {
                    cerr << "send login msg error: " << request << endl; 
                }
                else
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

            // if(len >= 1)
            // {
            //     cerr << "added group successfully!!!" << endl;
            //     pauseProgram();
            //     system("clear");
            // }

            char buffer[1024] = {0};
            len = recv(clientfd, buffer, 1024, 0);
            if(len > 1 && json::parse(buffer)["errno"] == 0)
            {
                cerr << "added group successfully!!!" << endl;
            }
            else
            {
                cerr << "added group failed!!!" << endl;
            }

        }break;
        case 5:
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

            // cerr << request_str << endl;

            int len = send(clientfd, request_str.c_str(), strlen(request_str.c_str()) + 1, 0);

            // if(len >= 1)
            // {
            //     cerr << "create group successfully!!!" << endl;
            //     pauseProgram();
            //     system("clear");
            // }

            char buffer[1024] = {0};
            len = recv(clientfd, buffer, 1024, 0);
            if(len > 1 && json::parse(buffer)["errno"] == 0)
            {
                cerr << "create group successfully!!!" << endl;
            }
            else
            {
                cerr << "create group failed!!!" << endl;
            }


        }break;
        default:
        {
            json js;
            js["msgid"] = 15;
            js["id"] = id;
            string request = js.dump();

            int len = send(clientfd, request.c_str(), strlen(request.c_str()) + 1, 0); 

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

    unordered_map<int, User> friend_list;

    json js;
    // {"msgid": 8, "id": 16}
    js["msgid"] = 8;
    js["id"] = id;

    string request = js.dump();

    // cerr << request << endl;

    int len = send(clientfd, request.c_str(), strlen(request.c_str()) + 1, 0);
    if(len == -1)
    {
        cerr << "send show friends msg error: " << request << endl;
    }
    else
    {
        char buffer[1024] = {0};
        len = recv(clientfd, buffer, 1024, 0);
        if(-1 == len)
        {
            cerr << "recv show friends response error" << endl;
        }
        else
        {
            int index = 0;
            json js = json::parse(buffer);
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

                    system("clear"); 
                    cerr << "==============================" << endl;
                    cerr << "Hello, " << name << " (" << id << ")" << endl;
                    cerr << "==============================" << endl;
                    cerr << "@ " << user.getName() << " " << user.getId() << ":" << endl;


                    std::thread readTask(readTaskHandler, clientfd);

                    while(chat_friend)
                    {          
                        chatMenu(user, name, id, clientfd, chat_friend);
                    }
                    system("clear"); 
                    json js;
                    js["msgid"] = 5;
                    js["id"] = id;
                    js["from"] = "";
                    js["to"] = id;
                    js["msg"] = "exit()";

                    string request = js.dump();

                    // cerr << request << endl;
                    int len = send(clientfd, request.c_str(), strlen(request.c_str()) + 1, 0);

                    readTask.join();

                    // groupMenu(id, name, chat_friend, clientfd);

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

    unordered_map<int, Group> group_list;

    string request_str = request.dump();
    int len = send(clientfd, request_str.c_str(), strlen(request_str.c_str()) + 1, 0);

    if(len < 1)
    {
        cerr << "show group failed!!! unknown error" << endl;
        pauseProgram();
        system("clear");
        return;
    }

    char buffer[1024] = {0};
    len = recv(clientfd, buffer, 1024, 0);
    
    int length = json::parse(buffer).size();
    int index = 0;
    for(auto item: json::parse(buffer))
    {
        index++;
        if(index == length)
        {
            break;
        }
        Group group = getMsgFromGroupString(item.dump());

        group_list.insert({group.getId(), group});

        
        cerr << item << endl;
    }
    cerr << length - 1 << " groups in total." << endl;  
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
            
            char buffer[1024] = {0};
            len = recv(clientfd, buffer, 1024, 0);
            if(-1 == len)
            {
                cerr << "recv group msg response error" << endl;
            }
            
            int length = json::parse(buffer).size();

            int index = 0;

            cerr << "group member: " << endl;

            for(auto it : json::parse(buffer))
            {   
                index++;
                if(index == length)
                {
                    break;
                }
                cerr << it << endl;
            }
            cerr << json::parse(buffer)["number"] << " member in total" << endl;

            cerr << "@ " << group.getName() << " " << group.getId() << ":" << endl;

            std::thread readTask(readGroupTaskHandler, clientfd, id);

            while(chat_group)
            {
                groupChatMenu(group, name, id, clientfd, chat_group);
            }

            // {"msgid": 12, "id": 17, "from": "van", "togroup": 2, "msg": "I am an artist"}
            // {"msgid": 5, "id": 19, "from": "bili", "to": 17, "msg": "wushuangdahuanggua"}
            system("clear"); 
            json js_exit;
            js_exit["msgid"] = 5;
            js_exit["id"] = id;
            js_exit["from"] = "";
            js_exit["to"] = id;
            js_exit["msg"] = "exit()";

            string request_exit = js_exit.dump();
            len = send(clientfd, request_exit.c_str(), strlen(request_exit.c_str()) + 1, 0);

            readTask.join();

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