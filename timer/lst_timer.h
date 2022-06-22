#ifndef LST_TIMER
#define LST_TIMER

#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>
#include <sys/stat.h>
#include <string.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <stdarg.h>
#include <errno.h>
#include <sys/wait.h>
#include <sys/uio.h>

#include <time.h>
#include "../log/log.h"



class Utils
{

  
public:
    Utils() {}
    ~Utils() {}
    
public:
    //对文件描述符设置非阻塞
   static int setnonblocking(int fd);

    //将内核事件表注册读事件，ET模式，选择开启EPOLLONESHOT
    static void addfd(int epollfd, int fd, bool one_shot, int TRIGMode);

    //信号处理函数
    static void sig_handler(int sig);

    //设置信号函数
    static void addsig(int sig, void(handler)(int), bool restart = true);
    static void show_error(int connfd, const char *info);
    
     //定时处理任务，重新定时以不断触发SIGALRM信号
    static void timer_handler();

public:
    static int *u_pipefd;
    static int u_epollfd;
};

#endif
