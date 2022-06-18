#include "webserver.h"

WebServer::WebServer()
{
    //http_conn类对象
    users = new http_conn[MAX_FD];

    //root文件夹路径
    char server_path[200];
    getcwd(server_path, 200);
    char root[6] = "/root";
    m_root = (char *)malloc(strlen(server_path) + strlen(root) + 1);
    strcpy(m_root, server_path);
    strcat(m_root, root);//deng: 把mroot和root字符串连接在一起。

    cout << "m_root == "<<m_root<<endl;

    //定时器
    users_timer = new client_data[MAX_FD];
}

WebServer::~WebServer()
{
    close(m_epollfd);
    close(m_listenfd);
    close(m_pipefd[1]);
    close(m_pipefd[0]);
    delete[] users;
    delete[] users_timer;
    delete m_pool;
}

void WebServer::init(int port, int opt_linger, int trigmode,  int thread_num,  int actor_model)
{
    m_port = port;
    m_thread_num = thread_num;
    m_OPT_LINGER = opt_linger;
    m_TRIGMode = trigmode;
    m_actormodel = actor_model;
}

void WebServer::trig_mode()
{
    //LT + LT
    if (0 == m_TRIGMode)
    {
        m_LISTENTrigmode = 0;
        m_CONNTrigmode = 0;
    }
    //LT + ET
    else if (1 == m_TRIGMode)
    {
        m_LISTENTrigmode = 0;
        m_CONNTrigmode = 1;
    }
    //ET + LT
    else if (2 == m_TRIGMode)
    {
        m_LISTENTrigmode = 1;
        m_CONNTrigmode = 0;
    }
    //ET + ET
    else if (3 == m_TRIGMode)
    {
        m_LISTENTrigmode = 1;
        m_CONNTrigmode = 1;
    }
}

void WebServer::log_write()
{

}

void WebServer::sql_pool()
{
    //     //初始化数据库连接池
    // m_connPool = connection_pool::GetInstance();
    // m_connPool->init("localhost", m_user, m_passWord, m_databaseName, 3306, m_sql_num);

    // //初始化数据库读取表
    // users->initmysql_result(m_connPool);
}

void WebServer::thread_pool()
{
    //线程池
    m_pool = new threadpool<http_conn>(m_actormodel, m_thread_num);
}

void WebServer::eventListen()
{
    //网络编程基础步骤
    m_listenfd = socket(PF_INET, SOCK_STREAM, 0);
    cout<< "m_listenfd == "<<m_listenfd<<endl;
    assert(m_listenfd >= 0);

    //优雅关闭连接, deng:正常不需要，用到在说。目的：设置一个close的延时时间，处理关闭时未发送完的数据
    if (0 == m_OPT_LINGER)
    {
        struct linger tmp = {0, 1};
        setsockopt(m_listenfd, SOL_SOCKET, SO_LINGER, &tmp, sizeof(tmp));
    }
    else if (1 == m_OPT_LINGER)
    {
        struct linger tmp = {1, 1};
        setsockopt(m_listenfd, SOL_SOCKET, SO_LINGER, &tmp, sizeof(tmp));
    }

    int ret = 0;
    struct sockaddr_in address;
    bzero(&address, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_ANY);
    address.sin_port = htons(m_port);

    int flag = 1;
    setsockopt(m_listenfd, SOL_SOCKET, SO_REUSEADDR, &flag, sizeof(flag));
    ret = bind(m_listenfd, (struct sockaddr *)&address, sizeof(address));
    assert(ret >= 0);
    ret = listen(m_listenfd, 5);
    assert(ret >= 0);

    utils.init(TIMESLOT);

    //epoll创建内核事件表
    epoll_event events[MAX_EVENT_NUMBER];
    m_epollfd = epoll_create(5);
    assert(m_epollfd != -1);

    /* deng:监听服务器 m_listenfd 的读事件 */
    utils.addfd(m_epollfd, m_listenfd, false, m_LISTENTrigmode);
    http_conn::m_epollfd = m_epollfd;

    // cout<< "m_pipefd[0] == "<<m_pipefd[0] <<"。  "<< "m_pipefd[1] == "<<m_pipefd[1]<<endl;

    ret = socketpair(PF_UNIX, SOCK_STREAM, 0, m_pipefd);
    //  cout<< "m_pipefd[0] == "<<m_pipefd[0] <<"。  "<< "m_pipefd[1] == "<<m_pipefd[1]<<endl;
    assert(ret != -1);
    utils.setnonblocking(m_pipefd[1]);
    /* deng:监听管道 m_pipefd 的读事件 */
    utils.addfd(m_epollfd, m_pipefd[0], false, 0);

    /* deng:注册这个信号好像没有用啊？ */
    // utils.addsig(SIGPIPE, SIG_IGN);

    utils.addsig(SIGALRM, utils.sig_handler, false); //闹钟信号 
    utils.addsig(SIGTERM, utils.sig_handler, false); //kill 的结束进程信号

    alarm(TIMESLOT);

    //工具类,信号和描述符基础操作
    Utils::u_pipefd = m_pipefd;
    Utils::u_epollfd = m_epollfd;
}

/* deng: 这个函数在接收到客户端的连接的时候调用，初始化 http连接对象 和 超时定时器 */
void WebServer::timer(int connfd, struct sockaddr_in client_address)
{
    users[connfd].init(connfd, client_address, m_root, m_CONNTrigmode);

    //初始化client_data数据
    //创建定时器，设置回调函数和超时时间，绑定用户数据，将定时器添加到链表中
    users_timer[connfd].address = client_address;
    users_timer[connfd].sockfd = connfd;
    util_timer *timer = new util_timer;
    timer->user_data = &users_timer[connfd];
    timer->cb_func = cb_func;
    time_t cur = time(NULL);
    timer->expire = cur + 3 * TIMESLOT;
    users_timer[connfd].timer = timer;
    utils.m_timer_lst.add_timer(timer);

     in_port_t client_port = ntohs( timer->user_data->address.sin_port);
    char client_ip[100] = {0};    //客户端ip
    inet_ntop(AF_INET, & timer->user_data->address.sin_addr.s_addr, client_ip, sizeof(client_ip));
    // int m_close_log = 0;
    // LOG_DEBUG("增加客户端连接：fd = %d, ip = %s, port = %d",  timer->user_data->sockfd, client_ip, client_port);
    static int count = 0;
    // printf("增加客户端连接：fd = %d, ip = %s, port = %d\r\n",  timer->user_data->sockfd, client_ip, client_port);
    // printf("客户端数量 = %d\r\n",  count++);
                                                                                            
}

//若有数据传输，则将定时器往后延迟3个单位
//并对新的定时器在链表上的位置进行调整
void WebServer::adjust_timer(util_timer *timer)
{
    time_t cur = time(NULL);
    timer->expire = cur + 3 * TIMESLOT;
    utils.m_timer_lst.adjust_timer(timer);

    LOG_DEBUG("%s", "adjust timer once");
}

void WebServer::deal_timer(util_timer *timer, int sockfd)
{
    timer->cb_func(&users_timer[sockfd]);
    if (timer)
    {
        utils.m_timer_lst.del_timer(timer);
    }

    LOG_INFO("close fd %d", users_timer[sockfd].sockfd);
}

bool WebServer::dealclinetdata()
{
    struct sockaddr_in client_address;
    socklen_t client_addrlength = sizeof(client_address);
    if (0 == m_LISTENTrigmode)
    {
        int connfd = accept(m_listenfd, (struct sockaddr *)&client_address, &client_addrlength);
        if (connfd < 0)
        {
            LOG_ERROR("%s:errno is:%d", "accept error", errno);
            return false;
        }
        if (http_conn::m_user_count >= MAX_FD)
        {
            /* deng: 不太明白意义何在，直接发送这句话给客户端 */
            utils.show_error(connfd, "Internal server busy");

            LOG_ERROR("%s", "Internal server busy");
            return false;
        }
        timer(connfd, client_address);
    }

    else
    {
        while (1)
        {
            int connfd = accept(m_listenfd, (struct sockaddr *)&client_address, &client_addrlength);
            if (connfd < 0)
            {
                LOG_ERROR("%s:errno is:%d", "accept error", errno);
                break;
            }
            if (http_conn::m_user_count >= MAX_FD)
            {
                utils.show_error(connfd, "Internal server busy");
                LOG_ERROR("%s", "Internal server busy");
                break;
            }
            timer(connfd, client_address);
        }
        return false;
    }
    return true;
}

bool WebServer::dealwithsignal(bool &timeout, bool &stop_server)
{
    int ret = 0;
    int sig;
    char signals[1024];
    ret = recv(m_pipefd[0], signals, sizeof(signals), 0);
    if (ret == -1)
    {
        return false;
    }
    else if (ret == 0)
    {
        return false;
    }
    else
    {
        for (int i = 0; i < ret; ++i)
        {
            // cout<< "管道接收到的信号：signals ==  "<< *((int *)signals)<<endl;
            switch (signals[i])
            {
            case SIGALRM:
            {
                // cout<< "超时"<<endl;
                timeout = true;
                break;
            }
            case SIGTERM:
            {
                // cout<< "停机"<<endl;
                stop_server = true;
                break;
            }
            }
        }
    }
    return true;
}

void WebServer::dealwithread(int sockfd)
{
    util_timer *timer = users_timer[sockfd].timer;

    //reactor 异步
    if (1 == m_actormodel)
    {
        if (timer)
        {
            adjust_timer(timer);
        }

        //若监测到读事件，将该事件放入请求队列
        m_pool->append(users + sockfd, 0);

        while (true)
        {
            //dneg: 数据执行完读取或写入操作会被置一。
            if (1 == users[sockfd].improv)
            {
                //deng: 数据读取或者写入失败会被置一。
                if (1 == users[sockfd].timer_flag)
                {
                    /* deng: 把定时器从列表中删除，并且释放相应的文件描述符 */
                    deal_timer(timer, sockfd);
                    users[sockfd].timer_flag = 0;
                }
                users[sockfd].improv = 0;
                break;
            }
        }
    }
    else
    {
        //proactor 同步
        if (users[sockfd].read_once())
        {
            LOG_DEBUG("proactor: 从客户端读取到数据");

            //若监测到读事件，将该事件放入请求队列
            m_pool->append_p(users + sockfd);

            if (timer)
            {
                adjust_timer(timer);
            }
        }
        else
        {
            deal_timer(timer, sockfd);
        }
    }
}

void WebServer::dealwithwrite(int sockfd)
{
    util_timer *timer = users_timer[sockfd].timer;
    //reactor
    if (1 == m_actormodel)
    {
        if (timer)
        {
            adjust_timer(timer);
        }

        m_pool->append(users + sockfd, 1);

        while (true)
        {
            if (1 == users[sockfd].improv)
            {
                if (1 == users[sockfd].timer_flag)
                {
                    deal_timer(timer, sockfd);
                    users[sockfd].timer_flag = 0;
                }
                users[sockfd].improv = 0;
                break;
            }                                                                                                                                                                                                                                                               
        }
    }
    else
    {
        //proactor
        if (users[sockfd].write())
        {
            LOG_INFO("send data to the client(%s)", inet_ntoa(users[sockfd].get_address()->sin_addr));

            if (timer)
            {
                adjust_timer(timer);
            }
        }
        else
        {
            deal_timer(timer, sockfd);
        }
    }
}

void WebServer::eventLoop()
{
    bool timeout = false;
    bool stop_server = false;

    long long count = 0;

    while (!stop_server)
    {

        int number = epoll_wait(m_epollfd, events, MAX_EVENT_NUMBER, -1);
        if (number < 0 && errno != EINTR)
        {
            LOG_ERROR("%s", "epoll failure");
            break;
        }

        for (int i = 0; i < number; i++)
        {

            int sockfd = events[i].data.fd;
            //处理新到的客户连接
            if (sockfd == m_listenfd)
            {
                bool flag = dealclinetdata();
                if (false == flag)
                    continue;
            }
            /* deng: EPOLLRDHUP : 对端断开连接 */
            else if (events[i].events & (EPOLLRDHUP | EPOLLHUP | EPOLLERR))
            {
                 LOG_INFO( "客户端先断开连接， 服务器也断开 ");
                //服务器端关闭连接，移除对应的定时器
                util_timer *timer = users_timer[sockfd].timer;
                deal_timer(timer, sockfd);
            }
            //处理信号
            else if ((sockfd == m_pipefd[0]) && (events[i].events & EPOLLIN))
            {
                // cout<<"进入处理信号"<<endl;

                /* deng: 处理信号 ：SIGALRM 和  SIGTERM， 
                （在 SIGALRM 和  SIGTERM 的回调函数中 给pipe发送数据，
                 触发管道的EPOLL读事件）。
                 然后被epoll监听到，在这里统一处理， 多了个中间商？？暂时没看到意义何在?*/
                bool flag = dealwithsignal(timeout, stop_server);
                // cout<<"flag = "<< flag <<endl;
                if (false == flag)
                    LOG_ERROR("%s", "dealclientdata failure");
            }
            //处理客户连接上接收到的数据
            else if (events[i].events & EPOLLIN)
            {
                dealwithread(sockfd);
            }
            else if (events[i].events & EPOLLOUT)
            {
                LOG_INFO( "dealwithwrite : 处理写");
                dealwithwrite(sockfd);
            }
        }
        
        if (timeout)
        {
            // utils.timer_handler();
            timeout = false;
        }

    }
    cout << "退出运行"<<endl;
}