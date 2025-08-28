#ifndef FRIENDMODEL_H
#define FRIENDMODEL_H

#include <vector>
#include "public.hpp"
#include "user.hpp"

using namespace std;

// Friend的数据操作类——增删改查等
class FriendModel
{
public:
    // FriendModel表的增加方法
    bool insert(int userid, int friendid);

    // 根据用户id查询用户信息
    vector<string> query(int usedid);
};

#endif