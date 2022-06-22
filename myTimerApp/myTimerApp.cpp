#include "myTimerApp.h"
#include "../webserver.h"


sort_timer_lst::sort_timer_lst()
{
    head = NULL;
    tail = NULL;
}
sort_timer_lst::~sort_timer_lst()
{
    util_timer *tmp = head;
    while (tmp)
    {
        head = tmp->next;
        delete tmp;
        tmp = head;
    }
}

void sort_timer_lst::add_timer(util_timer *timer)
{
    if (!timer)
    {
        return;
    }

    /* deng： 第一个添加的节点 */
    if (!head)
    {
        head = tail = timer;
        return;
    }

    /* 超时时间最小，直接作为头结点 */
    if (timer->expire < head->expire)
    {
        timer->next = head;
        head->prev = timer;
        head = timer;
        return;
    }
    add_timer(timer, head);
}
void sort_timer_lst::adjust_timer(util_timer *timer)
{
    if (!timer)
    {
        return;
    }
    util_timer *tmp = timer->next;
    /* deng: 有问题？ (timer->expire < tmp->expire), timer->expire 可能比他前面的要小，应该要调整顺序 */
    if (!tmp || (timer->expire < tmp->expire))
    {
        return;
    }
    if (timer == head)
    {
        /*deng: 删除节点 */
        head = head->next;
        head->prev = NULL;
        timer->next = NULL;

        /*deng:  插入节点 */
        add_timer(timer, head);
    }
    else
    {
        timer->prev->next = timer->next;
        timer->next->prev = timer->prev;
        add_timer(timer, timer->next);
    }
}
/* 写的代码有点多，可以优化 */
void sort_timer_lst::del_timer(util_timer *timer)
{
    if (!timer)
    {
        return;
    }
    if ((timer == head) && (timer == tail))
    {
        delete timer;
        head = NULL;
        tail = NULL;
        return;
    }
    if (timer == head)
    {
        head = head->next;
        head->prev = NULL;
        delete timer;
        return;
    }
    if (timer == tail)
    {
        tail = tail->prev;
        tail->next = NULL;
        delete timer;
        return;
    }
    timer->prev->next = timer->next;
    timer->next->prev = timer->prev;
    delete timer;
}
void sort_timer_lst::tick()
{
    if (!head)
    {
        return;
    }
    
    time_t cur = time(NULL);
    util_timer *tmp = head;

    /*deng:  一次可能有多个文件描述符的定时时间到了。 */
    while (tmp)
    {
        /* 没到定时时间 */
        if (cur < tmp->expire)
        {
            break;
        }

        /* deng： 去除客户端fd的所以监听，并关闭客户端连接 */
        tmp->cb_func(tmp->user_data);

        /*deng:  删除节点：从头结点依次往后删除 */
        head = tmp->next;
        if (head)
        {
            head->prev = NULL;
        }
        delete tmp;
        tmp = head;
    }
}

//deng: 复杂度 O（n）, 可以用优先级队列或平衡二叉树，O(logn)
void sort_timer_lst::add_timer(util_timer *timer, util_timer *lst_head)
{
    util_timer *prev = lst_head;
    util_timer *tmp = prev->next;
    while (tmp)
    {
        /* deng: 插入节点 */
        if (timer->expire < tmp->expire)
        {
            prev->next = timer;
            timer->next = tmp;
            tmp->prev = timer;
            timer->prev = prev;
            break;
        }
        prev = tmp;
        tmp = tmp->next;
    }

    /* deng: 插入节点是最后一个 */
    if (!tmp)
    {
        prev->next = timer;
        timer->prev = prev;
        timer->next = NULL;
        tail = timer;
    }
}




/* deng： 去除客户端fd的所以监听，并关闭客户端连接 */
void cb_func(client_data *user_data)
{
    epoll_ctl(Utils::u_epollfd, EPOLL_CTL_DEL, user_data->sockfd, 0);
    assert(user_data);


    // in_port_t client_port = ntohs(user_data->address.sin_port);
    // char client_ip[100] = {0};    //客户端ip
    // inet_ntop(AF_INET, &user_data->address.sin_addr.s_addr, client_ip, sizeof(client_ip));
    // int m_close_log = 0;
    // LOG_DEBUG("关闭客户端连接：fd = %d, ip = %s, port = %d", user_data->sockfd, client_ip, client_port);
    
    close(user_data->sockfd);
    http_conn::m_user_count--;
}


MyTimerApp:: MyTimerApp(int timeslot)
{
    m_TIMESLOT = timeslot;
         //定时器
    users_timer = new client_data[MAX_FD];
};

MyTimerApp:: ~MyTimerApp()
{
    delete[] users_timer;
}


/* deng: 这个函数在接收到客户端的连接的时候调用，初始化 http连接对象 和 超时定时器 */
void MyTimerApp::timer(int connfd, struct sockaddr_in client_address)
{

    //初始化client_data数据
    //创建定时器，设置回调函数和超时时间，绑定用户数据，将定时器添加到链表中
    users_timer[connfd].address = client_address;
    users_timer[connfd].sockfd = connfd;
    util_timer *timer = new util_timer;
    timer->user_data = &users_timer[connfd];
    timer->cb_func = cb_func;
    time_t cur = time(NULL);
    timer->expire = cur + 3 * m_TIMESLOT;
    users_timer[connfd].timer = timer;
    m_timer_lst.add_timer(timer);

    // in_port_t client_port = ntohs( timer->user_data->address.sin_port);
    // char client_ip[100] = {0};    //客户端ip
    // inet_ntop(AF_INET, & timer->user_data->address.sin_addr.s_addr, client_ip, sizeof(client_ip));
    // int m_close_log = 0;
    // LOG_DEBUG("增加客户端连接：fd = %d, ip = %s, port = %d",  timer->user_data->sockfd, client_ip, client_port);
    // static int count = 0;
    // printf("增加客户端连接：fd = %d, ip = %s, port = %d\r\n",  timer->user_data->sockfd, client_ip, client_port);
    // printf("客户端数量 = %d\r\n",  count++);
                                                                                            
}

//若有数据传输，则将定时器往后延迟3个单位
//并对新的定时器在链表上的位置进行调整
void MyTimerApp::adjust_timer(util_timer *timer)
{
    time_t cur = time(NULL);
    timer->expire = cur + 3 * m_TIMESLOT;
    m_timer_lst.adjust_timer(timer);

    LOG_DEBUG("%s", "adjust timer once");
}

void MyTimerApp::deal_timer(util_timer *timer, int sockfd)
{
    timer->cb_func(&users_timer[sockfd]);
    if (timer)
    {
        m_timer_lst.del_timer(timer);
    }

    LOG_INFO("close fd %d", users_timer[sockfd].sockfd);
}

//定时处理任务，重新定时以不断触发SIGALRM信号
void MyTimerApp::timer_handler()
{
    m_timer_lst.tick();
    alarm(m_TIMESLOT);
}