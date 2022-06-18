#include "mySqlApp.h"



MySqlApp::MySqlApp(string url, string User, string PassWord, string DataBaseName, int Port, int MaxConn)
{
    m_connPool = connection_pool::GetInstance();
    m_connPool->init(url, User, PassWord, DataBaseName, Port, MaxConn);
    initmysql_result(m_connPool);
}
MySqlApp:: ~MySqlApp()
{

}


void MySqlApp::initmysql_result(connection_pool *connPool)
{
    MYSQL *mysql = NULL;
    connectionRAII mysqlcon(&mysql, connPool);

    cout<< "进入 MySqlApp::initmysql_result"<<endl;

    //在user表中检索username，passwd数据，浏览器端输入
    if (mysql_query(mysql, "SELECT username,passwd FROM user"))
    {
         cout<< "进入mysql_query"<<endl;
        LOG_ERROR("SELECT error:%s\n", mysql_error(mysql));
    }

    //从表中检索完整的结果集
    MYSQL_RES *result = mysql_store_result(mysql);

    //返回结果集中的列数
    int num_fields = mysql_num_fields(result);

    //返回所有字段结构的数组
    MYSQL_FIELD *fields = mysql_fetch_fields(result);

    //从结果集中获取下一行，将对应的用户名和密码，存入map中
    while (MYSQL_ROW row = mysql_fetch_row(result))
    {
        string temp1(row[0]);
        string temp2(row[1]);
        users_table[temp1] = temp2;
    }
}

void MySqlApp::traverse_userTable()
{
    int count = 0;
    cout<< "进入traverse_userTable" <<endl;
    for(map<string, string>::iterator it = users_table.begin(); it != users_table.end(); it++, count++)
    {
        cout<< "第 "<< count<<" 个 ： 账号 = "<< it->first << "密码 = "<< it->second <<endl;
    }
}

/* 通过用户名查找是否有这个账号， false =  没有。 true = 有 */
bool MySqlApp::findAccount(string username)
{
    return users_table.find(username) != users_table.end();
}

/* 确定账号密码是否正确， false = 错误 。 true = 正确 */
bool MySqlApp::checkAccountAndpasswd(string username, string passwd)
{
    /* 得先确保有没有这个账号， */
    if(users_table.find(username) != users_table.end())
    {
        return users_table[username] == passwd;
    }
    return false;
}

/* 往数据库里添加账号密码, ret = 0 成功，其他失败*/
int MySqlApp::addAccountAndpasswd(string username, string passwd)
{
    char sql_insert[200] = {0};
    sprintf(sql_insert, "INSERT INTO user(username, passwd) VALUES('%s','%s')", username.c_str(), passwd.c_str());
    MYSQL *mysql = NULL;
    connectionRAII mysqlcon(&mysql, m_connPool);

    m_lock.lock();
    int res = mysql_query(mysql, sql_insert);
    if(!res)
        users_table.insert(pair<string, string>(username, passwd));

    m_lock.unlock();
    return res;
}