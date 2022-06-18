#ifndef HTTPCONNECTION_H
#define HTTPCONNECTION_H
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
#include <map>

#include "../lock/locker.h"
#include "../CGImysql/sql_connection_pool.h"
#include "../timer/lst_timer.h"
#include "../log/log.h"
#include "../CGImysql/mySqlApp.h"

class http_conn
{
public:
    static const int FILENAME_LEN = 200;
    static const int READ_BUFFER_SIZE = 2048;
    static const int WRITE_BUFFER_SIZE = 1024;
    static int m_epollfd; //
    static int m_user_count;//deng: 记录连接服务器用户的数量
    static MySqlApp* m_mySqlApp;
public:
    enum METHOD
    {
        GET = 0,
        POST,
        HEAD,
        PUT,
        DELETE,
        TRACE,
        OPTIONS,
        CONNECT,
        PATH
    };
    enum CHECK_STATE
    {
        CHECK_STATE_REQUESTLINE = 0,
        CHECK_STATE_HEADER,
        CHECK_STATE_CONTENT
    };
    enum HTTP_CODE
    {
        NO_REQUEST,
        GET_REQUEST,
        BAD_REQUEST,
        NO_RESOURCE,
        FORBIDDEN_REQUEST,  //文件不可读
        FILE_REQUEST,       //请求文件
        INTERNAL_ERROR,  //
        CLOSED_CONNECTION
    };
    enum LINE_STATUS
    {
        LINE_OK = 0,
        LINE_BAD,
        LINE_OPEN  //表示读取的数据没有以\r\n结尾，读取的数据还不完整。
    };
public:
    http_conn() {}
    ~http_conn() {}

public:
    void init(int sockfd, const sockaddr_in &addr, char *, int);
    void close_conn(bool real_close = true);
    void process();
    bool read_once(); 
    bool write();
    sockaddr_in *get_address()
    {
        return &m_address;
    }
    int timer_flag;
    int improv;

private:
    void init();
    HTTP_CODE process_read();
    bool process_write(HTTP_CODE ret);
    HTTP_CODE parse_request_line(char *text);
    HTTP_CODE parse_headers(char *text);
    HTTP_CODE parse_content(char *text);
    HTTP_CODE do_request();
    char *get_line() { return m_read_buf + m_start_line; };
    LINE_STATUS parse_line();
    void unmap();
    bool add_response(const char *format, ...);
    bool add_content(const char *content);
    bool add_status_line(int status, const char *title);
    bool add_headers(int content_length);
    bool add_content_type();
    bool add_content_length(int content_length);
    bool add_linger();
    bool add_blank_line();

public:

    int m_state;  //读为0, 写为1
private:
     struct http_decodeInfo{
        METHOD      method;            //请求的方法
        char        *url;               //请求的url
        char        *version;           //请求的http版本
        char        *Host;              //请求的主机地址，ip + port
        int         content_length;     //提交内容长度
        bool        keep_alive;         //deng: 是否和客户端保持长连接。
        char        *content_string;    //存储请求头数据,提交的内容

        char        request_file_path[FILENAME_LEN];    //客户端的url请求对应的服务器资源完整路径。
        char        *request_file_mmapAddress;          //请求资源文件的mmap映射的地址
        struct stat request_file_stat;                  //请求资源文件的属性
    };
    http_decodeInfo  m_http_decodeInfo;   //这个是http协议解析的信息集合


    char m_read_buf[READ_BUFFER_SIZE];//deng: 读取数据的缓冲区
    int m_read_idx;                   //deng: 读取数据的索引，也就是当前读取的数量
    int m_checked_idx;
    int m_start_line;
    CHECK_STATE m_check_state;

    char m_write_buf[WRITE_BUFFER_SIZE];
    int m_write_idx;
    struct iovec m_iv[2];
    int m_iv_count;
    
    map<string, string> m_users;

    /* 以下是初始化需要传入的参数。 */
    char *doc_root;  //
    int m_TRIGMode;

    /* 这两个参数包含了客户端的所有参数 */
    int m_sockfd;
    sockaddr_in m_address;

    
};

#endif
