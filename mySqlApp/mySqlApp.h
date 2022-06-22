#ifndef _mySqlApp_h
#define _mySqlApp_h

#include "../CGImysql/sql_connection_pool.h"
#include <fstream>
#include <map>


class MySqlApp
{
public:
    MySqlApp(string url, string User, string PassWord, string DataBaseName, int Port, int MaxConn);
    ~MySqlApp();
public:
    void    traverse_userTable();
    bool    findAccount(string username);
    bool    checkAccountAndpasswd(string username, string passwd);
    int     addAccountAndpasswd(string username, string passwd);
private:
    void initmysql_result(connection_pool *connPool);
private:
    connection_pool* m_connPool;       //数据库连接池
    map<string, string> users_table;  // 保存用户账号密码的表
    locker m_lock;
};

#endif