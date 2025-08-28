#include "offlinemessagemodel.hpp"
#include "db.h"

#include <iostream>
#include <string>
using namespace std;

// OfflineMessage表的增加方法
bool OfflineMessageModel::insert(int userid, string message)
{
    char sql[1024] = {0};
    sprintf(sql, "insert into offlinemessage value(%d, '%s')", userid, message.c_str());

    MySQL mysql;
    if(mysql.connect())
    {
        if(mysql.update(sql))
        {
            return true;
        }
    }
    return false;
}

// 根据用户号码信息查询用户信息
vector<string> OfflineMessageModel::query(int userid)
{
    char sql[1024] = {0};
    sprintf(sql, "select message from offlinemessage where userid = %d", userid);
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
                result.push_back(row[0]);
            }
        }
        mysql_free_result(res);
        return result;
    }
    return result;
} 

// 删除数据库中的消息(用户上线读取后)
void OfflineMessageModel::erase(int userid)
{
    char sql[1024] = {0};
    sprintf(sql, "delete from offlinemessage where userid = %d", userid);

    MySQL mysql;
    if(mysql.connect())
    {
        mysql.update(sql);
    }

    return;
}