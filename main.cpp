#include "config.h"
#include "./mySqlApp/mySqlApp.h"
#include "./myTimerApp/myTimerApp.h"
#include "./myTimer/myTimer.h"
#include "./myHeap/myHeap.h"


#define MAX_SIZE  10
void * timer1_callback(void *arg)
{
    // struct timeval tv;
    // gettimeofday(&tv, NULL);
    // cout<< "timer2_callback: tv.tv_sec = "<< tv.tv_sec << "。 tv.tv_usec = " << tv.tv_usec <<endl;

    static struct timeval old_tv[MAX_SIZE];
    struct timeval tv, temp_tv;
    gettimeofday(&tv, NULL);
    long long index = (long long)arg;

    unsigned long long epoll_wait_timeout = (tv.tv_sec - old_tv[index].tv_sec )*1000000 +  (tv.tv_usec - old_tv[index].tv_usec );

    temp_tv.tv_sec = epoll_wait_timeout/1000000 ;
    temp_tv.tv_usec = (epoll_wait_timeout%1000000) ;

     cout<< "定时器 "<< index <<" : temp_tv.tv_sec = "<< temp_tv.tv_sec << "。 temp_tv.tv_usec = " << temp_tv.tv_usec/1000 <<endl;

    old_tv[index] = tv;
}

int main(int argc, char *argv[])
{
     
    MyTimer* tiemr[MAX_SIZE];

    for(int i = 0; i < MAX_SIZE; i++)
    {
        tiemr[i] = new MyTimer(500*(i + 1),   timer1_callback, (void *)i);
        tiemr[i]->start();
    }


    while(1)
    {
        sleep(5);
        cout<<"我还活着"<<endl;
    }

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
    config.TRIGMode = 3;
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

    //定时器
    MyTimerApp myTimerApp = MyTimerApp();
    server.m_myTimerApp = &myTimerApp;
    


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