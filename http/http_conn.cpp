#include "http_conn.h"

#include <mysql/mysql.h>
#include <fstream>

//定义http响应的一些状态信息
const char *ok_200_title = "OK";
const char *error_400_title = "Bad Request";
const char *error_400_form = "Your request has bad syntax or is inherently impossible to staisfy.\n";
const char *error_403_title = "Forbidden";
const char *error_403_form = "You do not have permission to get file form this server.\n";
const char *error_404_title = "Not Found";
const char *error_404_form = "The requested file was not found on this server.\n";
const char *error_500_title = "Internal Error";
const char *error_500_form = "There was an unusual problem serving the request file.\n";




int http_conn::m_user_count = 0;
int http_conn::m_epollfd = -1;
MySqlApp* http_conn::m_mySqlApp = NULL;



//对文件描述符设置非阻塞
int setnonblocking(int fd)
{
    int old_option = fcntl(fd, F_GETFL);
    int new_option = old_option | O_NONBLOCK;
    fcntl(fd, F_SETFL, new_option);
    return old_option;
}
//将内核事件表注册读事件，ET模式，选择开启EPOLLONESHOT
void addfd(int epollfd, int fd, bool one_shot, int TRIGMode)
{
    epoll_event event;
    event.data.fd = fd;

    if (1 == TRIGMode)
        event.events = EPOLLIN | EPOLLET | EPOLLRDHUP;
    else
        event.events = EPOLLIN | EPOLLRDHUP;

    if (one_shot)
        event.events |= EPOLLONESHOT;
    epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event);
    setnonblocking(fd);
}
//从内核时间表删除描述符
void removefd(int epollfd, int fd)
{
    epoll_ctl(epollfd, EPOLL_CTL_DEL, fd, 0);
    close(fd);
}
//将事件重置为EPOLLONESHOT
void modfd(int epollfd, int fd, int ev, int TRIGMode)
{
    epoll_event event;
    event.data.fd = fd;

    if (1 == TRIGMode)
        event.events = ev | EPOLLET | EPOLLONESHOT | EPOLLRDHUP;
    else
        event.events = ev | EPOLLONESHOT | EPOLLRDHUP;

    epoll_ctl(epollfd, EPOLL_CTL_MOD, fd, &event);
}

//deng: false: 写缓冲区的数据满了。
bool http_conn::add_response(const char *format, ...)
{
    if (m_write_idx >= WRITE_BUFFER_SIZE)
        return false;
    va_list arg_list;
    va_start(arg_list, format);
    int len = vsnprintf(m_write_buf + m_write_idx, WRITE_BUFFER_SIZE - 1 - m_write_idx, format, arg_list);
    if (len >= (WRITE_BUFFER_SIZE - 1 - m_write_idx))
    {
        va_end(arg_list);
        return false;
    }
    m_write_idx += len;
    va_end(arg_list);

    LOG_INFO("request:%s", m_write_buf);

    return true;
}
bool http_conn::add_status_line(int status, const char *title)
{
    return add_response("%s %d %s\r\n", "HTTP/1.1", status, title);
}
bool http_conn::add_headers(int content_len)
{
    return add_content_length(content_len) && add_linger() &&
           add_blank_line();
}
bool http_conn::add_content_length(int content_len)
{
    return add_response("Content-Length:%d\r\n", content_len);
}
bool http_conn::add_content_type()
{
    return add_response("Content-Type:%s\r\n", "text/html");
}
bool http_conn::add_linger()
{
    return add_response("Connection:%s\r\n", (m_http_decodeInfo.keep_alive == true) ? "keep-alive" : "close");
}
bool http_conn::add_blank_line()
{
    return add_response("%s", "\r\n");
}
bool http_conn::add_content(const char *content)
{
    return add_response("%s", content);
}


//关闭连接，关闭一个连接，客户总量减一
void http_conn::close_conn(bool real_close)
{
    if (real_close && (m_sockfd != -1))
    {
        printf("close %d\n", m_sockfd);
        removefd(m_epollfd, m_sockfd);
        m_sockfd = -1;
        m_user_count--;
    }
}

//初始化新接受的连接
//check_state默认为分析请求行状态
void http_conn::init()
{
    
    
    m_check_state = CHECK_STATE_REQUESTLINE;
    m_start_line = 0;
    m_checked_idx = 0;
    m_read_idx = 0;
    m_write_idx = 0;

    m_state = 0;
    timer_flag = 0;
    improv = 0;

    memset(m_read_buf, '\0', READ_BUFFER_SIZE);
    memset(m_write_buf, '\0', WRITE_BUFFER_SIZE);
    memset(&m_http_decodeInfo, '\0', sizeof(m_http_decodeInfo));
    memset(m_iv, '\0', sizeof(m_iv));

}

//初始化连接,外部调用初始化套接字地址
void http_conn::init(int sockfd, const sockaddr_in &addr, char *root, int TRIGMode)
{
    m_sockfd = sockfd;


    /* 在这个地方添加EPOLL事件，感觉很奇怪 */
    addfd(m_epollfd, sockfd, true, m_TRIGMode);
    m_user_count++;

    //当浏览器出现连接重置时，可能是网站根目录出错或http响应格式出错或者访问的文件中内容完全为空
    doc_root = root;
    m_TRIGMode = TRIGMode;

    init();
}

//从状态机，用于分析出一行内容
//返回值为行的读取状态，有LINE_OK,LINE_BAD,LINE_OPEN
http_conn::LINE_STATUS http_conn::parse_line()
{
    char temp;
    for (; m_checked_idx < m_read_idx; ++m_checked_idx)
    {
        temp = m_read_buf[m_checked_idx];
        if (temp == '\r')
        {
            if ((m_checked_idx + 1) == m_read_idx)//deng: 缓冲区的内容已经读完了。
                return LINE_OPEN;
            else if (m_read_buf[m_checked_idx + 1] == '\n')
            {
                m_read_buf[m_checked_idx++] = '\0';
                m_read_buf[m_checked_idx++] = '\0';
                return LINE_OK;
            }
            return LINE_BAD;
        }
        else if (temp == '\n') 
        {
            if (m_checked_idx > 1 && m_read_buf[m_checked_idx - 1] == '\r')//dneg: 上次读取到\r就没了。
            {
                m_read_buf[m_checked_idx - 1] = '\0';
                m_read_buf[m_checked_idx++] = '\0';
                return LINE_OK;
            }
            return LINE_BAD;
        }
    }
    return LINE_OPEN;
}

//解析http请求行，获得请求方法，目标url及http版本号
http_conn::HTTP_CODE http_conn::parse_request_line(char *text)
{
    char *temp;
    /*deng : strpbrk : 返回指向字符串str2中的任意字符第一次出现在字符串str1中的位置的指针；如果str1中没有与str2相同的字符，那么返回NULL。*/
    temp = strpbrk(text, " \t");
    if (!temp)
    {
        return BAD_REQUEST;
    }
    *temp++ = '\0';
    if (strcasecmp(text, "GET") == 0)
    {
        m_http_decodeInfo.method = GET;
    }
    else if (strcasecmp(text, "POST") == 0)
    {
        m_http_decodeInfo.method = POST;
       
    }
    else
        return BAD_REQUEST;

    /* deng: strspn :检索字符串 str1 中第一个不在字符串 str2 中出现的字符下标 */
    temp += strspn(temp, " \t"); 
    char * versiontemp = strpbrk(temp, " \t");
    if (!versiontemp)
        return BAD_REQUEST;
    *versiontemp++ = '\0';                    //temp 此时 temp 指向资源内容。
    m_http_decodeInfo.version = versiontemp +  strspn(versiontemp, " \t"); // deng: 跳过 空格 和 \t， 此时 m_http_decodeInfo.version 为http版本的首地址。
    if (strcasecmp(m_http_decodeInfo.version, "HTTP/1.1") != 0)
        return BAD_REQUEST;

    //deng : http请求
    if (strncasecmp(temp, "http://", 7) == 0)
    {
        temp += 7;
        //该函数返回在字符串 str 中第一次出现字符 c 的位置，如果未找到该字符则返回 NULL
        temp = strchr(temp, '/');
    }
    //deng : https请求
    else if (strncasecmp(temp, "https://", 8) == 0)
    {
        temp += 8;
        temp = strchr(temp, '/');
    }

    if (!temp || temp[0] != '/')
        return BAD_REQUEST;

    m_http_decodeInfo.url = temp; //deng:的到请求的资源url

    // //当url为/时，显示判断界面
    // if (strlen(temp) == 1)
    //     strcat(temp, "judge.html");//degn: strcat :把str2(包括'\0')拷贝到str1的尾部(连接)，并返回str1。其中终止原str1的'\0'被str2的第一个字符覆盖。
    m_check_state = CHECK_STATE_HEADER;
    return NO_REQUEST;
}

//解析http请求的一个头部信息
http_conn::HTTP_CODE http_conn::parse_headers(char *text)
{
    if (text[0] == '\0') // deng: 已经到最后一个空行。 转态只在这里改变
    {
        if ( m_http_decodeInfo.content_length != 0) //表明有提交的内容
        {
            m_check_state = CHECK_STATE_CONTENT;
            return NO_REQUEST;
        }
        return GET_REQUEST;
    }
    else if (strncasecmp(text, "Accept:", sizeof("Accept:") - 1) == 0)
    {
        // text += sizeof("Accept:") - 1;
        // text += strspn(text, " \t");
    }
    else if (strncasecmp(text, "Accept-Encoding:", sizeof("Accept-Encoding:") - 1) == 0){}
    else if (strncasecmp(text, "Accept-Language:", sizeof("Accept-Language:") - 1) == 0){}
    else if (strncasecmp(text, "Cache-Control:", sizeof("Cache-Control:") - 1) == 0){}
    else if (strncasecmp(text, "Content-Type:", sizeof("Content-Type:") - 1) == 0){}
    else if (strncasecmp(text, "Origin:", sizeof("Origin:") - 1) == 0){}
    else if (strncasecmp(text, "Referer:", sizeof("Referer:") - 1) == 0){}
    else if (strncasecmp(text, "Upgrade-Insecure-Requests:", sizeof("Upgrade-Insecure-Requests:") - 1) == 0){}
    else if (strncasecmp(text, "User-Agent:", sizeof("User-Agent:") - 1) == 0){}
    else if (strncasecmp(text, "Connection:", sizeof("Connection:") - 1) == 0)
    {
        text += sizeof("Connection:") - 1;
        text += strspn(text, " \t");
        if (strcasecmp(text, "keep-alive") == 0)
        {
            
            m_http_decodeInfo.keep_alive = true;
        }
    }
    else if (strncasecmp(text, "Proxy-Connection:", sizeof("Proxy-Connection:") - 1) == 0)
    {
        text += sizeof("Proxy-Connection:") - 1;
        text += strspn(text, " \t");
        if (strcasecmp(text, "keep-alive") == 0)
        {
            m_http_decodeInfo.keep_alive = true;
        }
    }
    else if (strncasecmp(text, "Content-length:", sizeof("Content-length:") - 1) == 0)
    {
        text += sizeof("Content-length:") - 1;
        text += strspn(text, " \t");
        m_http_decodeInfo.content_length = atol(text);
    }
    else if (strncasecmp(text, "Host:", sizeof("Host:") - 1) == 0)
    {
        text += sizeof("Host:") - 1;
        text += strspn(text, " \t");
        m_http_decodeInfo.Host = text;
    }
    else
    {
        LOG_INFO("oop!unknow header: %s", text);
    }
    return NO_REQUEST;
}

//判断http请求是否被完整读入
http_conn::HTTP_CODE http_conn::parse_content(char *text)
{
    //如果小于，可能是数据还没有接收完
    if (m_read_idx >= ( m_http_decodeInfo.content_length + m_checked_idx))
    {
        text[ m_http_decodeInfo.content_length] = '\0';
        //POST请求中最后为输入的用户名和密码
         m_http_decodeInfo.content_string = text;
        return GET_REQUEST;
    }
    return NO_REQUEST;
}

http_conn::HTTP_CODE http_conn::do_request()

{
    char* request_filePath = m_http_decodeInfo.request_file_path;
    char tempUrl[100] = {0};
    strcpy(tempUrl, m_http_decodeInfo.url);

    strcpy(request_filePath, doc_root);
    int len = strlen(doc_root);
    

    //printf("tempUrl:%s\n", tempUrl);
    //C 库函数 char *strrchr(const char *str, int c) 在参数 str 所指向的字符串中搜索最后一次出现字符 c（一个无符号字符）的位置。
    const char *p = strrchr(tempUrl, '/');

    // cout<< "m_http_decodeInfo.url == " << m_http_decodeInfo.url<<endl;
    // cout << "*(p + 1) == " <<*(p + 1)<< endl;

    if(strcasecmp(tempUrl, "/") == 0)
    {
        strcpy(tempUrl, "/judge.html");
    }
    //处理cgi
    else if (m_http_decodeInfo.method == POST && (*(p + 1) == '2' || *(p + 1) == '3'))
    {
        //将用户名和密码提取出来
        //user=123&passwd=123
        char name[100], password[100];
        int i;
        for (i = 5;  m_http_decodeInfo.content_string[i] != '&'; ++i)
            name[i - 5] =  m_http_decodeInfo.content_string[i];
        name[i - 5] = '\0';

        int j = 0;
        for (i = i + 10;  m_http_decodeInfo.content_string[i] != '\0'; ++i, ++j)
            password[j] =  m_http_decodeInfo.content_string[i];
        password[j] = '\0';

        // printf("username = %s, passwd = %s\r\n", name, password);
        if (*(p + 1) == '3')//deng： 用户点击注册操作
        {
            //如果是注册，先检测数据库中是否有重名的
            //没有重名的，进行增加数据

            if(!m_mySqlApp->findAccount(name))
            {
                if (!m_mySqlApp->addAccountAndpasswd(name, password))
                    strcpy(tempUrl, "/log.html");
                else
                    strcpy(tempUrl, "/registerError.html");
            }
            else
                strcpy(tempUrl, "/registerError.html");
        }
        //如果是登录，直接判断
        //若浏览器端输入的用户名和密码在表中可以查找到，返回1，否则返回0
        else if (*(p + 1) == '2') //dneg: (用户点击登录操作)
        {
            if (m_mySqlApp->checkAccountAndpasswd(name, password))
                strcpy(tempUrl, "/welcome.html");
            else
                strcpy(tempUrl, "/logError.html");
        }

        // m_mySqlApp->traverse_userTable();
    }
    else if (*(p + 1) == '0') // deng: （用户点击新用户）发送注册界面给客户
    {
        strcpy(tempUrl,"/register.html");
    }
    else if (*(p + 1) == '1')// deng: （用户点击已有账户）发送登录界面给客户
    {
         strcpy(tempUrl,"/log.html");
    }
    else if (*(p + 1) == '5') //deng: 登录后用户点击 “图片”
    {
        strcpy(tempUrl,"/picture.html");
    }
    else if (*(p + 1) == '6')//deng: 登录后用户点击 “视频”
    {
        strcpy(tempUrl,"/video.html");
    }
    else if (*(p + 1) == '7')//deng: 登录后用户点击 ”关注我“
    {
        strcpy(tempUrl,"/fans.html");
    }

    strncpy(request_filePath + len, tempUrl, FILENAME_LEN - len - 1);
    

    cout<< "request_filePath " << request_filePath<<endl;

    if (stat(request_filePath, &m_http_decodeInfo.request_file_stat) < 0)
        return NO_RESOURCE;

    if (!(m_http_decodeInfo.request_file_stat.st_mode & S_IROTH))//deng: 文件不可读
        return FORBIDDEN_REQUEST;

    if (S_ISDIR(m_http_decodeInfo.request_file_stat.st_mode)) //deng： 是一个目录
        return BAD_REQUEST;

    int fd = open(request_filePath, O_RDONLY);
    m_http_decodeInfo.request_file_mmapAddress = (char *)mmap(0, m_http_decodeInfo.request_file_stat.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
    close(fd);
    return FILE_REQUEST;
}
void http_conn::unmap()
{
    if (m_http_decodeInfo.request_file_mmapAddress)
    {
        munmap(m_http_decodeInfo.request_file_mmapAddress, m_http_decodeInfo.request_file_stat.st_size);
        m_http_decodeInfo.request_file_mmapAddress = 0;
    }
}
//循环读取客户数据，直到无数据可读或对方关闭连接
//非阻塞ET工作模式下，需要一次性将数据读完
/* false: 读取数据失败 或者 客户端断开连接 */
bool http_conn::read_once()
{
    //deng: 超过缓冲区大小
    if (m_read_idx >= READ_BUFFER_SIZE)
    {
        printf("http_conn::read_once() 读取数据失败: 超过缓冲区大小\r\n");
        return false;
    }
    int bytes_read = 0;

    //LT读取数据
    if (0 == m_TRIGMode)
    {
        LOG_DEBUG("LT读数据");
        bytes_read = recv(m_sockfd, m_read_buf + m_read_idx, READ_BUFFER_SIZE - m_read_idx, 0);
        m_read_idx += bytes_read;

        if (bytes_read <= 0)
        {
            return false;
        }

        return true;
    }
    //ET读数据
    else
    {
        LOG_DEBUG("ET读数据");
        while (true)
        {
            /* deng: 读缓冲区里的内容如果大于READ_BUFFER_SIZE， 会怎样？先不管 */
            bytes_read = recv(m_sockfd, m_read_buf + m_read_idx, READ_BUFFER_SIZE - m_read_idx, 0);
            if (bytes_read == -1)
            {
                /* 表示数据读完了 */
                if (errno == EAGAIN || errno == EWOULDBLOCK)
                {
                     break;
                }
                   
                printf("http_conn::read_once()  : read error\r\n");
                return false;
            }
            else if (bytes_read == 0) //deng: 客户端断开连接
            {
                /* 当缓冲区的数据超过 2048， 会进入这里， 导致关闭客户端 */
                printf("客户端断开连接， m_read_idx = %d\r\n", m_read_idx);
                return false;
            }
            m_read_idx += bytes_read;
        }
        return true;
    }
}

//false: 表示写数据失败 或者 客户端不是长链接每次操作后要断开连接
bool http_conn::write()
{
    int temp = 0;
    //dneg; 表示没有数据要传输
    if (m_iv[0].iov_len == 0 && m_iv[1].iov_len == 0)
    {
        modfd(m_epollfd, m_sockfd, EPOLLIN, m_TRIGMode);
        init();
        return true;
    }
    static int count = 0;
    // printf("发送：第 %d 条内容\r\n", ++count);
    LOG_INFO("发送：第 %d 条内容\r\n", ++count);

    // printf("%s", m_write_buf);
    //  printf("%s", m_http_decodeInfo.request_file_mmapAddress);
    while (1)
    {
        temp = writev(m_sockfd, m_iv, m_iv_count);

        if (temp < 0)
        {
            if (errno == EAGAIN) //deng: 内核写缓冲区满了，等客户端读取数据后才可以写。
            {
                modfd(m_epollfd, m_sockfd, EPOLLOUT, m_TRIGMode);//deng： 第一次注册（每次调用这个函数）或 从不可写到可写，调用 epooll_wait 会触发一次写事件。
                return true;
            }
            unmap();
            return false;
        }

        // cout<< "A_m_iv[0].iov_len == "<< m_iv[0].iov_len <<endl;
        // cout<< "A_m_iv[1].iov_len == "<< m_iv[1].iov_len <<endl;

        //如果m_iv[0]里的内容较少，一次传输会把 m_iv[0] 和 m_iv[1] 里的内容一起传输
        if(m_iv[0].iov_len > 0)
        {
            if(m_iv[0].iov_len < temp )
            {
                m_iv[1].iov_len -= temp - m_iv[0].iov_len;
                m_iv[1].iov_base =  (char *)m_iv[1].iov_base + temp - m_iv[0].iov_len;
                m_iv[0].iov_len = 0;
            }else{
                m_iv[0].iov_len -= temp;
                m_iv[0].iov_base =  (char *)m_iv[0].iov_base + temp;
            }
            
        }else if(m_iv[1].iov_len > 0){
            m_iv[1].iov_len -= temp;
            m_iv[1].iov_base =  (char *)m_iv[1].iov_base + temp;
        }
        // cout<< "temp == "<< temp <<endl;
        // cout<< "B_m_iv[0].iov_len == "<< m_iv[0].iov_len <<endl;
        // cout<< "B_m_iv[1].iov_len == "<< m_iv[1].iov_len <<endl;

        //deng: 表示数据传输完毕
        if (m_iv[0].iov_len <= 0 && m_iv[1].iov_len <= 0)
        {
            unmap();
            modfd(m_epollfd, m_sockfd, EPOLLIN, m_TRIGMode);

            if (m_http_decodeInfo.keep_alive)// deng: 保持连接
            {
                init();
                return true;
            }
            else
            {
                return false;
            }
        }
    }
}

//deng : 调用这个函数的时候数据已经 读取到读缓冲区里了。
http_conn::HTTP_CODE http_conn::process_read()
{
    LINE_STATUS line_status = LINE_OK;
    HTTP_CODE ret = NO_REQUEST;
    char *text = 0;

    static int count = 0;
    // printf("\r\n接收：第 %d 条内容\r\n", ++count);
    LOG_INFO("\r\n接收：第 %d 条内容\r\n", ++count);
    // printf("%s", m_read_buf);

    while ((m_check_state == CHECK_STATE_CONTENT && line_status == LINE_OK) || ((line_status = parse_line()) == LINE_OK))
    {
        text = get_line();// deng: 得到刚才解析成功的一行数据
        m_start_line = m_checked_idx; //deng: 确定下一行的起始地址
        // LOG_INFO("%s", text);
        
        switch (m_check_state)
        {
        case CHECK_STATE_REQUESTLINE:
        {
            ret = parse_request_line(text);
            if (ret == BAD_REQUEST)
                return BAD_REQUEST;
            break;
        }
        case CHECK_STATE_HEADER:
        {
            ret = parse_headers(text);
            if (ret == BAD_REQUEST)
                return BAD_REQUEST;
            else if (ret == GET_REQUEST)
            {
                return do_request();
            }
            break;
        }
        case CHECK_STATE_CONTENT: //deng: 客户端有提交的数据才会执行到这一步
        {
            ret = parse_content(text);
            if (ret == GET_REQUEST)
                return do_request();
            line_status = LINE_OPEN;
            break;
        }
        default:
            return INTERNAL_ERROR;
        }
    }
    return NO_REQUEST;
}
bool http_conn::process_write(HTTP_CODE ret)
{
    switch (ret)
    {
    case INTERNAL_ERROR:
    {
        add_status_line(500, error_500_title);
        add_headers(strlen(error_500_form));
        if (!add_content(error_500_form))
            return false;
        break;
    }
    case BAD_REQUEST:
    {
        add_status_line(404, error_404_title);
        add_headers(strlen(error_404_form));
        if (!add_content(error_404_form))
            return false;
        break;
    }
    case FORBIDDEN_REQUEST:
    {
        add_status_line(403, error_403_title);
        add_headers(strlen(error_403_form));
        if (!add_content(error_403_form))
            return false;
        break;
    }
    case FILE_REQUEST:
    {
        add_status_line(200, ok_200_title);
        if (m_http_decodeInfo.request_file_stat.st_size != 0)
        {
            add_headers(m_http_decodeInfo.request_file_stat.st_size);
            m_iv[0].iov_base = m_write_buf;
            m_iv[0].iov_len = m_write_idx;
            m_iv[1].iov_base = m_http_decodeInfo.request_file_mmapAddress;
            m_iv[1].iov_len = m_http_decodeInfo.request_file_stat.st_size;
            m_iv_count = 2;
            return true;
        }
        else
        {
            const char *ok_string = "<html><body></body></html>";
            add_headers(strlen(ok_string));
            if (!add_content(ok_string))
                return false;
        }
    }
    default:
        return false;
    }
    m_iv[0].iov_base = m_write_buf;
    m_iv[0].iov_len = m_write_idx;
    m_iv_count = 1;
    return true;
}
void http_conn::process()
{
    HTTP_CODE read_ret = process_read();

    //deng: NO_REQUEST :直接什么都不回
    if (read_ret == NO_REQUEST)
    {
        modfd(m_epollfd, m_sockfd, EPOLLIN, m_TRIGMode);
        return;
    }
    bool write_ret = process_write(read_ret);
    if (!write_ret)
    {
        close_conn();
    }
    modfd(m_epollfd, m_sockfd, EPOLLOUT, m_TRIGMode);
}
