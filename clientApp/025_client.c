#include<stdio.h>
#include<unistd.h>
#include<sys/socket.h>
#include<stdlib.h>
#include<arpa/inet.h> //sockaddr_in
#include<string.h>  //strlen
#include "pthread.h"
#include "errno.h"
#include "signal.h"

#define SERVER_PORT 9600
#define SERVER_IP  "120.24.61.9"

#define MAX_READ_CHILD_THREAD_COUNT  1
#define MAX_WRITE_CHILD_THREAD_COUNT  2

typedef struct{
    int num;
    pthread_t thread_id;
    int client_Socket;
}thread_exit_info_t;


void * readThreadCallback(void * arg);
void * writeThreadCallback(void * arg);
FILE* getLog_File(void);


int getClientSocket(void)
{
    int client_Socket = 0;
    struct sockaddr_in serv_addr = {0};
    socklen_t serv_Len = {0};
    int ret = 0;

    /* 一、 socket,创建客户端socket */
    client_Socket = socket(AF_INET,SOCK_STREAM,0);
    if(client_Socket == -1)
    {
        printf("socket error \n");
        exit(1);
    }
    printf("client_Socket == %d \n", client_Socket);

    /* 二、connect 连接服务器socket */
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(SERVER_PORT);
    inet_pton(AF_INET,SERVER_IP,(void *)&serv_addr.sin_addr.s_addr);
    serv_Len = sizeof(serv_addr);

    ret = connect(client_Socket,(struct sockaddr *)&serv_addr,serv_Len);
    if(ret == -1)
    {
        printf("connect error \n");
        exit(1);
    }
  
    printf("connect : ret == %d \n", ret);
    return client_Socket;
}

int client_Socket = 0;
/* 互斥量 */
pthread_mutex_t mutex;

void my_SIGPIPE_signalCallBack(int data)
{
    printf("进入my_SIGPIPE_signalCallBack, data = %d\r\n", data);
}

int main(void)
{
    pthread_t tid[1024] = {0};
    thread_exit_info_t thread_exit_info[1024];
    thread_exit_info_t *p = NULL;

    pthread_mutex_init(&mutex,NULL);


    // fprintf(getLog_File(),"niaho%d\r\n", 100);
    client_Socket = getClientSocket();
    signal(SIGPIPE, my_SIGPIPE_signalCallBack);

     /* 创建线程 */
    for(int i = 0; i < MAX_READ_CHILD_THREAD_COUNT; i++)
    {
        thread_exit_info[i].num = i;
        thread_exit_info[i].client_Socket = client_Socket;
        pthread_create(&tid[i], NULL, readThreadCallback, &thread_exit_info[i]);
    }

    for(int i = 0; i < MAX_WRITE_CHILD_THREAD_COUNT; i++)
    {
        thread_exit_info[i + MAX_READ_CHILD_THREAD_COUNT].num = i;
        thread_exit_info[i + MAX_READ_CHILD_THREAD_COUNT].client_Socket = client_Socket;
        pthread_create(&tid[i + MAX_READ_CHILD_THREAD_COUNT], NULL, writeThreadCallback, &thread_exit_info[i + MAX_READ_CHILD_THREAD_COUNT]);
    }

    /* 回收线程 */
    for(int i = 0; i < MAX_READ_CHILD_THREAD_COUNT + MAX_WRITE_CHILD_THREAD_COUNT; i++)
    {
        
        pthread_join(tid[i], (void **)&p);
        printf("线程回收: 线程号 = %d ，id = 0x%lx\r\n", p->num, p->thread_id);
        fprintf(getLog_File(),"线程回收: 线程号 = %d ，id = 0x%lx\r\n", p->num, p->thread_id);
        getLog_File();
    }


    pthread_mutex_destroy(&mutex);
    fprintf(getLog_File(),"主线程id = 0x%x\r\n",(unsigned int)pthread_self());
    printf("主线程id = 0x%x\r\n",(unsigned int)pthread_self());
    return 0;
}

void * readThreadCallback(void * arg)
{
    int i = 0;
    int temp = 0;
    thread_exit_info_t *thread_exit_info = (thread_exit_info_t *)arg;
    thread_exit_info->thread_id = pthread_self();
    printf(" --------------read 线程号 = %d,线程id= 0x%lx\r\n",
    thread_exit_info->num, thread_exit_info->thread_id);

    // fprintf(getLog_File()," --------------read 线程号 = %d,线程id= 0x%lx\r\n",
    // thread_exit_info->num, thread_exit_info->thread_id);
    // getLog_File();
//     fprintf(getLog_File(),"niahoaaaaa%d\r\n", 100);
//     getLog_File();
//  printf("进入生产者线程1\r\n");

    int ret = 0;
    int n = 0;
    char write_Buf[1024] = {0};
    while(1)
    {
        break;
      /* 四、read,读服务器发送来的数据 */
        n  = read(client_Socket,write_Buf,sizeof(write_Buf));
        printf("read n , n = %d, client_Socket = %d\n", n, client_Socket);

        if(client_Socket == 0)
                    break;
        // printf("read n == %d\r\n", n);
        if(n == -1)
        {

            printf("read error , errno = %d\n", errno);

             pthread_mutex_lock(&mutex);
             printf("read : pthread_mutex_lock\r\n");

            /* 五、 close, 关闭客户端socket */
            close(client_Socket);
            client_Socket = 0;
             pthread_mutex_unlock(&mutex);

              sleep(5);

            pthread_mutex_lock(&mutex);
            client_Socket = getClientSocket();
            pthread_mutex_unlock(&mutex);
        //    break;
        }
        else if(n == 0)
        {
            printf("服务器关闭了连接\r\n");

             pthread_mutex_lock(&mutex);
             /* 五、 close, 关闭客户端socket */
            close(client_Socket);
            client_Socket = 0;
            pthread_mutex_unlock(&mutex);
            //  break;

            sleep(5);

            pthread_mutex_lock(&mutex);
            client_Socket = getClientSocket();
            pthread_mutex_unlock(&mutex);
           
        }
        else
        {
            fprintf(getLog_File(),"读取到服务器数据%s\r\n", write_Buf);
            write(STDOUT_FILENO,write_Buf,n); //输出到控制台
        }
            
    }
    return arg;
}
void * writeThreadCallback(void * arg)
{
    int i = 0;
    int temp = 0;
    thread_exit_info_t *thread_exit_info = (thread_exit_info_t *)arg;
    thread_exit_info->thread_id = pthread_self();
    
    printf(" ----------write 线程号 = %d,线程id= 0x%lx\r\n",
    thread_exit_info->num, thread_exit_info->thread_id);

    // fprintf(getLog_File()," --------------write 线程号 = %d,线程id= 0x%lx\r\n",
    // thread_exit_info->num, thread_exit_info->thread_id);
    // getLog_File();

    int ret = 0;
    char write_Buf[2048] = {0};

    switch(thread_exit_info->num)
    {
        case 0:
            // printf("write case 0\r\n");
            while(1)
            {
                break;
                /* 三、write,给服务器发数据 */
                fgets(write_Buf,sizeof(write_Buf),stdin); //读取键盘输入数据
                if(client_Socket == 0)
                {
                    continue;
                    break;
                }
                   

                 ret = write(client_Socket,write_Buf,strlen(write_Buf));

                // fprintf(getLog_File(), "write info == %s\r\n", write_Buf);
                // getLog_File();

                if(ret == -1)
                {
                    printf("write error \n");
                    /* 五、 close, 关闭客户端socket */
                    close(client_Socket);
                    client_Socket = 0;
                    break;
                }
                
            }
            break;
        case 1:
            // printf("write case 1\r\n");
            // char str[] = "心跳包\r\n";
             char str[] = 
            "GET / HTTP/1.1\r\n\
Host: 120.24.61.9:9600\r\n\
User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/99.0.4844.82 Safari/537.36\r\n\
Accept: text/html,application/xhtml+xml,application/xml;q=0.9,image/avif,image/webp,image/apng,*/*;q=0.8,application/signed-exchange;v=b3;q=0.9\r\n\
Accept-Encoding: gzip, deflate\r\n\
Accept-Language: en-US,en;q=0.9,zh-CN;q=0.8,zh;q=0.7\r\n\
Cache-Control: max-age=0\r\n\
Proxy-Connection: keep-alive\r\n\
Connection: keep-alive\r\n\
Upgrade-Insecure-Requests: 1\r\n\r\n";
            memcpy(write_Buf, str, sizeof(str));
            int count = 0;
            while(1)
            {
                // sleep(5);
                // printf("write 1\r\n");
                if(client_Socket == 0)
                {
                    // printf("write 3\r\n");
                    continue;
                    break;
                }

                pthread_mutex_lock(&mutex); 
                printf("第 %d 次发送数据到服务器\r\n", count++);
                usleep(100*1000);
                //  sleep(1);
                ret = write(client_Socket,write_Buf,strlen(write_Buf));
                pthread_mutex_unlock(&mutex);
                // printf("write ret == %d\r\n", ret);
                // fprintf(getLog_File(), "write info == %s\r\n", write_Buf);
                // getLog_File();

                if(ret == -1)
                {
                    printf("write error 1, client_Socket = %d, errno == %d\n", client_Socket, errno);
                    sleep(1);

                    pthread_mutex_lock(&mutex);
                    /* 五、 close, 关闭客户端socket */
                    close(client_Socket);
                    client_Socket = 0;
                    pthread_mutex_unlock(&mutex);
                    //  break;

                    sleep(5);

                    pthread_mutex_lock(&mutex);
                    client_Socket = getClientSocket();
                    pthread_mutex_unlock(&mutex);
                    /* 五、 close, 关闭客户端socket */
                    // close(client_Socket);
                    // client_Socket = 0;
                    // break;
                }
                // else if(ret == 0)
                // {
                    printf("ret == %d, errno = %d\r\n",ret, errno);
                // }

            }
            // printf("write 4\r\n");
            break;

    }
    
    printf("退出write线程， thread_exit_info->num == %d\r\n", thread_exit_info->num);
    return arg;
}



FILE* getLog_File(void)
{
    static FILE* mf = NULL;
    static char log_full_name[256] = {0};
    if(mf)
    {
        fflush(mf);
        fclose(mf);
 
        // printf("fileName ==%s  \r\n", log_full_name );
        return mf = fopen(log_full_name, "a");
    }
        
    const char *file_name =  "myClinetLog";
    time_t t = time(NULL);
    struct tm *sys_tm = localtime(&t);
    struct tm my_tm = *sys_tm;

    memset(log_full_name, 0, sizeof(log_full_name));
    snprintf(log_full_name, 255, "%d_%02d_%02d_%s", my_tm.tm_year + 1900, my_tm.tm_mon + 1, my_tm.tm_mday, file_name);
/* 参数1： ①不带路径，表示打开当前目录下的文件，（-a: 表示在文件末尾追加内容，如果找不到文件则创建文件）
                ②带路径，则打开具体路径的文件
     */
    mf = fopen(log_full_name, "a");
    printf("2 ==%d,  mf->_fileno = %d  \r\n", mf->_mode, mf->_fileno);
    return mf;

}