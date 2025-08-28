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

#endif