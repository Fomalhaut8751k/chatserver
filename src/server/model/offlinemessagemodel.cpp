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


// #################################### v2 #########################################
bool OfflineMessageModel_v2::insert(int userid, int friendid, int groupid, string message)
{
    char sql[1024] = {0};
    sprintf(sql, "insert into offlinemessage_v2 value(%d, %d, %d, '%s')", userid, friendid, groupid, message.c_str());

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

// 根据用户号码以及好友id信息查询用户消息
vector<string> OfflineMessageModel_v2::query_for_friend(int userid, int friendid)  // 不能放松空白消息
{
    char sql[1024] = {0};
    sprintf(sql, "select message from offlinemessage_v2 where userid = %d and friendid = %d", userid, friendid);
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

// 根据用户号码以及群聊id信息查询群聊消息
vector<string> OfflineMessageModel_v2::query_for_group(int userid, int groupid)  // 不能放松空白消息
{
    char sql[1024] = {0};
    sprintf(sql, "select message from offlinemessage_v2 where userid = %d and groupid = %d", userid, groupid);
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

// 删除数据库中指定好友的消息(用户上线读取后)
void OfflineMessageModel_v2::erase_for_friend(int userid, int friendid)
{
    char sql[1024] = {0};
    sprintf(sql, "delete from offlinemessage_v2 where userid = %d and friendid = %d", userid, friendid);

    MySQL mysql;
    if(mysql.connect())
    {
        mysql.update(sql);
    }

    return;
}

// 删除数据库中指定群聊的消息(用户上线读取后)
void OfflineMessageModel_v2::erase_for_group(int userid, int groupid)
{
    char sql[1024] = {0};
    sprintf(sql, "delete from offlinemessage_v2 where userid = %d and groupid = %d", userid, groupid);

    MySQL mysql;
    if(mysql.connect())
    {
        mysql.update(sql);
    }

    return;
}