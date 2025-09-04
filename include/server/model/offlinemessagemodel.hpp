#ifndef OFFLINEMESSAGEMODEL_H 
#define OFFLINEMESSAGEMODEL_H

#include <string>
#include <vector>

using namespace std;

// OfflineMessage的数据操作类——增删改查等
class OfflineMessageModel
{
public:
    // OfflineMessage表的增加方法
    bool insert(int userid, string message);

    // 根据用户号码信息查询用户信息
    vector<string> query(int userid);  // 不能放松空白消息

    // 删除数据库中的消息(用户上线读取后)
    void erase(int userid);
};

// 新的OfflineMessage的数据操作类——增删改查等
class OfflineMessageModel_v2
{
public:
    // OfflineMessage_v2表的增加方法
    bool insert(int userid, int friendid, int groupid, string message);

    // 根据用户号码以及好友id信息查询用户消息
    vector<string> query_for_friend(int userid, int friendid);  // 不能放松空白消息

    // 根据用户号码以及群聊id信息查询群聊消息
    vector<string> query_for_group(int userid, int groupid);  // 不能放松空白消息

    // 删除数据库中指定好友的消息(用户上线读取后)
    void erase_for_friend(int userid, int friendid);

    // 删除数据库中指定群聊的消息(用户上线读取后)
    void erase_for_group(int userid, int groupid);
};
#endif