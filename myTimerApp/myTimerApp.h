#ifndef _myTimerApp_h
#define _myTimerApp_h


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

class util_timer;

struct client_data
{
    sockaddr_in address;
    int sockfd;
    util_timer *timer;
};

class util_timer
{
public:
    util_timer() : prev(NULL), next(NULL) {}

public:
    time_t expire; //deng：超时时间
    void (* cb_func)(client_data *);  //dneg: 超时回调函数

    client_data *user_data; // deng: 用来：/* deng： 去除客户端fd的所以监听，并关闭客户端连接 */
    util_timer *prev;
    util_timer *next;
};

class sort_timer_lst
{
public:
    sort_timer_lst();
    ~sort_timer_lst();

    void add_timer(util_timer *timer);
    void adjust_timer(util_timer *timer);
    void del_timer(util_timer *timer);
    void tick();

private:
    void add_timer(util_timer *timer, util_timer *lst_head);

    util_timer *head;
    util_timer *tail;
};



class MyTimerApp
{ 
public:
    // MyTimerApp();
    MyTimerApp(int timeslot  = 5);
    ~MyTimerApp();
public:
    void timer(int connfd, struct sockaddr_in client_address);
    void adjust_timer(util_timer *timer);
    void deal_timer(util_timer *timer, int sockfd);
private:
    //定时处理任务，重新定时以不断触发SIGALRM信号
    void timer_handler();
public:
    client_data *users_timer;
private:
    // Utils utils;
    int m_TIMESLOT;
    sort_timer_lst m_timer_lst;

};

#endif