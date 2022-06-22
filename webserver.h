#ifndef WEBSERVER_H
#define WEBSERVER_H

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <cassert>
#include <sys/epoll.h>

#include "./threadpool/threadpool.h"
#include "./http/http_conn.h"

#include "./myTimerApp/myTimerApp.h"

const int MAX_FD = 65536;           //最大文件描述符
const int MAX_EVENT_NUMBER = 10000; //最大事件数


class WebServer
{
public:
    WebServer();
    ~WebServer();

    void init(int port ,  int opt_linger, int trigmode, int thread_num,  int actor_model);

    void thread_pool();
    void trig_mode();

    void eventListen();
    void eventLoop();

    bool dealclinetdata();
    bool dealwithsignal(bool& timeout, bool& stop_server);
    void dealwithread(int sockfd);
    void dealwithwrite(int sockfd);

public:
    //基础
    int m_port;
    char *m_root;
    int m_actormodel;  //react（io异步） 主线程负责 ：监听读写和连接事件。                  工作线程负责： 读操作、写操作。
                      //preact（io同步） 主线程负责监 ：听读写和连接事件、读操作、写操作。   工作线程负责：业务处理

    int m_pipefd[2];
    int m_epollfd;
    http_conn *users;

    //线程池相关
    threadpool<http_conn> *m_pool;
    int m_thread_num;

    //epoll_event相关
    epoll_event events[MAX_EVENT_NUMBER];

    int m_listenfd;
    int m_OPT_LINGER;
    int m_TRIGMode;
    int m_LISTENTrigmode;
    int m_CONNTrigmode;

    //定时器相关
    MyTimerApp *m_myTimerApp;

};
#endif
