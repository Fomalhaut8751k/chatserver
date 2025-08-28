#include "groupmodel.hpp"
#include "db.h"


// 创建群组
bool GroupModel::createGroup(Group& group)
{
    char sql[1024] = {0};
    string group_name = group.getName();
    string group_desc = group.getDesc();

    sprintf(sql, "insert into allgroups(groupname, groupdssc) values('%s', '%s')", group_name.c_str(), group_desc.c_str());

    MySQL mysql;
    if(mysql.connect())
    {
        if(mysql.update(sql))
        {
            group.setId(mysql_insert_id(mysql.getConnection()));
            return true;
        }
    }
    return false;
}

// 加入群组
bool GroupModel::addGroup(int userid, int groupid, string role)
{
    char sql[1024] = {0};
    sprintf(sql, "insert into groupuser(groupid, userid, grouprole) values(%d, %d, '%s')", groupid, userid, role.c_str());

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

// 查询用户所在群聊信息
vector<Group> GroupModel::queryGroups(int userid)
{
    char sql[1024] = {0};
    sprintf(sql, "select a.id, a.groupname, a.groupdssc from allgroups a inner join \
     groupuser b on b.groupid = a.id where b.userid = %d", userid);

    vector<Group> result;

    MySQL mysql;
    if(mysql.connect())
    {
        MYSQL_RES* res = mysql.query(sql);
        if(res != nullptr)
        {
            MYSQL_ROW row;
            while((row = mysql_fetch_row(res)) != nullptr)
            {
                result.emplace_back(Group(atoi(row[0]), row[1], row[2]));
            }
        }
        mysql_free_result(res);
        return result;
    }
    return vector<Group>();
}

// 根据指定的groupid查询群组用户id列表，主要用户群聊业务给群组其他成员群发消息
vector<GroupUser> GroupModel::queryGroupUsers(int groupid)
{
    char sql[1024] = {0};
    /*
        先查询groupuser表中满足groupid的数据，然后根据数据中userid在user表中查询数据
    */
    sprintf(sql, "select a.id, a.name, a.state, b.grouprole from user a inner join \
     groupuser b on b.userid = a.id where b.groupid = %d;", groupid);

    vector<GroupUser> result; 

    MySQL mysql;
    if(mysql.connect())
    {
        MYSQL_RES* res = mysql.query(sql);
        if(res != nullptr)
        {
            MYSQL_ROW row;
            while((row = mysql_fetch_row(res)) != nullptr)
            {
                GroupUser group_user;
                group_user.setId(atoi(row[0]));
                group_user.setName(row[1]);
                group_user.setState(row[2]);
                group_user.setRole(row[3]);

                result.emplace_back(group_user);
            }
        }
        mysql_free_result(res);
        return result;
    }
    return vector<GroupUser>();
} 
