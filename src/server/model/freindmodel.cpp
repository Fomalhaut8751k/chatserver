#include "friendmodel.hpp"
#include "db.h"

#include <iostream>
using namespace std;

// FriendModel表的增加方法
bool FriendModel::insert(int userid, int friendid)
{
    bool insert_label = false;
    bool insert_label_reverse = false;

    char sql[1024] = {0};
    sprintf(sql, "insert into friend value(%d, %d)", userid, friendid);

    MySQL mysql1;
    if(mysql1.connect())
    {
        if(mysql1.update(sql))
        {
            insert_label = true;
        }
    }

    if(!insert_label)
    {
        return false;
    }

    sql[1024] = {0};
    sprintf(sql, "insert into friend value(%d, %d)", friendid, userid);

    MySQL mysql2;
    if(mysql2.connect())
    {
        if(mysql2.update(sql))
        {
            insert_label_reverse = true;
        }
    }

    return insert_label && insert_label_reverse;
}

// 根据用户id查询用户信息
vector<string> FriendModel::query(int userid)
{
    char sql[1024] = {0};
    // 查找用户的id, 名字，以及状态
    sprintf(sql, "select a.id, a.name, a.state from user a inner join friend b on b.friendid = a.id where b.userid = %d;", userid);
    vector<string> result;

    MySQL mysql;
    if(mysql.connect())
    {
        MYSQL_RES* res = mysql.query(sql);
        if(res != nullptr)
        {
            MYSQL_ROW row;
            while((row = mysql_fetch_row(res)) != nullptr)
            {
                string friend_message = " id: ";
                friend_message += row[0];
                friend_message += " name: ";
                friend_message += row[1];
                friend_message += " state: ";
                friend_message += row[2]; 
                
                result.emplace_back(friend_message);
            }
        }
        mysql_free_result(res);
        return result;
    }
    return result;
}
