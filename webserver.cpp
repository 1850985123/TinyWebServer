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

   
}

WebServer::~WebServer()
{
    close(m_epollfd);
    close(m_listenfd);
    close(m_pipefd[1]);  
    close(m_pipefd[0]);
    delete[] users;
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


    //epoll创建内核事件表
    epoll_event events[MAX_EVENT_NUMBER];
    m_epollfd = epoll_create(5);
    assert(m_epollfd != -1);

    /* deng:监听服务器 m_listenfd 的读事件 */
    Utils::addfd(m_epollfd, m_listenfd, false, m_LISTENTrigmode);
    http_conn::m_epollfd = m_epollfd;

    // cout<< "m_pipefd[0] == "<<m_pipefd[0] <<"。  "<< "m_pipefd[1] == "<<m_pipefd[1]<<endl;

    ret = socketpair(PF_UNIX, SOCK_STREAM, 0, m_pipefd);
    //  cout<< "m_pipefd[0] == "<<m_pipefd[0] <<"。  "<< "m_pipefd[1] == "<<m_pipefd[1]<<endl;
    assert(ret != -1);
    Utils::setnonblocking(m_pipefd[1]);
    /* deng:监听管道 m_pipefd 的读事件 */
    Utils::addfd(m_epollfd, m_pipefd[0], false, 0);

    /* deng:注册这个信号好像没有用啊？: 
        有用，解决客户端关闭后调用write导致进程关闭的问题， 
        设置这个信号进程不会关闭，write会返回 -1*/
    Utils::addsig(SIGPIPE, SIG_IGN);

    Utils::addsig(SIGALRM, Utils::sig_handler, false); //闹钟信号 
    
    Utils::addsig(SIGTERM, Utils::sig_handler, false); //kill 的结束进程信号



    //工具类,信号和描述符基础操作
    Utils::u_pipefd = m_pipefd;
    Utils::u_epollfd = m_epollfd;
}


bool WebServer::dealclinetdata()
{
    struct sockaddr_in client_address;
    socklen_t client_addrlength = sizeof(client_address);

    cout<< "m_LISTENTrigmode == "<< m_LISTENTrigmode <<endl;

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
            Utils::show_error(connfd, "Internal server busy");

            LOG_ERROR("%s", "Internal server busy");
            return false;
        }

        users[connfd].init(connfd, client_address, m_root, m_CONNTrigmode);
        m_myTimerApp->timer(connfd, client_address);
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
                Utils::show_error(connfd, "Internal server busy");
                LOG_ERROR("%s", "Internal server busy");
                break;
            }
            
            users[connfd].init(connfd, client_address, m_root, m_CONNTrigmode);
            m_myTimerApp->timer(connfd, client_address);
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
    util_timer *timer = m_myTimerApp->users_timer[sockfd].timer;

    //reactor 
    if (1 == m_actormodel)
    {
        if (timer)
        {
             m_myTimerApp->adjust_timer(timer);
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
                     m_myTimerApp->deal_timer(timer, sockfd);
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
        if (users[sockfd].read_once())
        {
            LOG_DEBUG("proactor: 从客户端读取到数据");

            //若监测到读事件，将该事件放入请求队列
            m_pool->append(users + sockfd, 0);

            if (timer)
            {
                 m_myTimerApp->adjust_timer(timer);
            }
        }
        else
        {
             m_myTimerApp->deal_timer(timer, sockfd);
        }
    }
}

void WebServer::dealwithwrite(int sockfd)
{
    util_timer *timer = m_myTimerApp->users_timer[sockfd].timer;
    //reactor
    if (1 == m_actormodel)
    {
        if (timer)
        {
             m_myTimerApp->adjust_timer(timer);
        }

        m_pool->append(users + sockfd, 1);

        while (true)
        {
            if (1 == users[sockfd].improv)
            {
                if (1 == users[sockfd].timer_flag)
                {
                    m_myTimerApp->deal_timer(timer, sockfd);
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
                 m_myTimerApp->adjust_timer(timer);
            }
        }
        else
        {
             m_myTimerApp->deal_timer(timer, sockfd);
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
                util_timer *timer = m_myTimerApp->users_timer[sockfd].timer;
                m_myTimerApp->deal_timer(timer, sockfd);
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
        
        // if (timeout)
        // {
        //     Utils::timer_handler();
        //     timeout = false;
        // }

    }
    cout << "退出运行"<<endl;
}