#include "config.h"
#include "./CGImysql/mySqlApp.h"

int main(int argc, char *argv[])
{
    //需要修改的数据库信息,登录名,密码,库名
    // string user = "root";
    // string passwd = "root";
    string user = "root";
    string passwd = "";
    string databasename = "yourdb";

    //命令行解析
    Config config;
    config.parse_arg(argc, argv);

    WebServer server;
    cout << "nihao "<<endl;

    //初始化,通过默认配置或者命令行参数初始化服务器
    config.LOGWrite = 1; //异步写日志
    server.init(config.PORT,  config.OPT_LINGER, config.TRIGMode,  config.thread_num, config.actor_model);
    

    //日志，初始化日志，设置是同步写还是异步写日志。
    if (0 == config.close_log) // deng: 日志打开进入
    {
        //初始化日志
        if (1 == config.LOGWrite)//deng: 异步写入日志
        {
            cout<< "异步写入日志 "<<endl;
             Log::get_instance()->init(LOG_FIRST_DIR_NAME, config.close_log, 800000*2, 800);//"./ServerLog"
        } 
        else
        {
             cout<< "同步写入日志 "<<endl;
             Log::get_instance()->init(LOG_FIRST_DIR_NAME, config.close_log, 800000*2, 0);
        }
           
    }


    //数据库
    //初始化数据库
    MySqlApp mySqlApp =  MySqlApp("localhost", user, passwd, databasename, 3306, config.sql_num);
    http_conn::m_mySqlApp= &mySqlApp;

    //线程池
    server.thread_pool();


    //触发模式
    server.trig_mode();

    //监听
    server.eventListen();

    //运行
    server.eventLoop();
    

    return 0;
}