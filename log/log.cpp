#include <string.h>
#include <time.h>
#include <sys/time.h>
#include <stdarg.h>
#include "log.h"
#include <pthread.h>

#include <sys/stat.h>// deng add 2022_6_16
#include <sys/types.h>// deng add 2022_6_16

using namespace std;

Log::Log()
{
    m_count = 0;
    m_is_async = false; // deng: 默认同步写日志
    memset(m_buf, '\0', sizeof(m_buf));
    memset(m_first_dir_name, '\0', sizeof(m_first_dir_name));
    memset(m_second_dir_name, '\0', sizeof(m_second_dir_name));

    // char * levelList[]  = LOG_STRING_LEVEL_COUNTS;
    // m_log_string = levelList;
    // int levelCount = sizeof(levelList)/sizeof(char *);
    // cout << levelCount<<endl;

    // levelCount = sizeof(m_log_string)/sizeof(char *);
    // cout << levelCount<<endl;


    // // m_log_kinds
    // cout << sizeof(temp)<<endl;
    // cout << temp[0]<<endl;
    // cout << temp[1]<<endl;
    // cout << temp[2]<<endl;
    // cout << temp[3]<<endl;
    // cout << temp[4]<<endl;
    // cout << sizeof(m_log_string)<<endl;
    // cout << m_log_string[1]<<endl;
}

Log::~Log()
{
    for(int i = DEBUG; i <  LOG_LEVLE_MAX; i++)
    {
        if (m_fp[i] != NULL)
        {
            fclose(m_fp[i]);
        }
    }
    
}

/* deng: 异步方式： 
        日志产生--》 把日志添加到队列--》由日志处理线程统一处理队列里的日志 ，把日志写入到文件
   同步方式： 
        日志产生--》处理日志，写入文件
*/
//异步需要设置阻塞队列的长度，同步不需要设置
/* max_queue_size : 异步日志阻塞队列的大小
   split_lines ： 分文件存储的最大行数
   file_name ： 日志的根目录路径
   close_log ： 控制日志打开或关闭的外部开关。
 */
bool Log::init(const char *dir_name, int close_log, int split_lines, int max_queue_size)
{
    //如果设置了max_queue_size,则设置为异步
    if (max_queue_size >= 1)
    {
        m_is_async = true;
        m_log_queue = new block_queue<LogInfo>(max_queue_size);
        pthread_t tid;
        //flush_log_thread为回调函数,这里表示创建线程异步写日志
        pthread_create(&tid, NULL, flush_log_thread, NULL);
    }

    m_close_log = close_log;
    m_split_lines = split_lines;

    time_t t = time(NULL);
    struct tm *sys_tm = localtime(&t);
    struct tm my_tm = *sys_tm;

    strcpy(m_first_dir_name, dir_name ); //deng: log_name = 日志名
    //先得到一级目录名
    int ret = mkdir(dir_name, S_IRUSR|S_IWUSR); //dneg:所有者有读写权限
    cout<< "mkdir ret == "<< ret << endl;

    //在得到二级目录名
    char tempFullDirName[256] = {0};
    snprintf(m_second_dir_name, 256, "%s/%d_%02d_%02d/", m_first_dir_name, my_tm.tm_year + 1900, my_tm.tm_mon + 1, my_tm.tm_mday);

    // cout<< m_dir_name <<endl;
    //得到完整的目录名，创建目录。
    ret = mkdir(m_second_dir_name, S_IRUSR|S_IWUSR); //dneg:所有者有读写权限
    // cout<< "mkdir2 ret == "<< ret << endl;
    m_today = my_tm.tm_mday;

    openLogFile("DEBUG");
    openLogFile("INFO");
    openLogFile("WARN");
    openLogFile("ERROR");
    openLogFile("ALL");
    return true;
}
bool Log::openLogFile(const char * logLevel)
{
     /* 参数1： ①不带路径，表示打开当前目录下的文件，（-a: 表示在文件末尾追加内容，如果找不到文件则创建文件）
                ②带路径，则打开具体路径的文件
     */
    LOG_LEVLE level;
    if (strcasecmp("DEBUG", logLevel) == 0)
    {
        level = DEBUG;
    }else if(strcasecmp("INFO", logLevel) == 0)
    {
        level = INFO;
    }
    else  if(strcasecmp("WARN", logLevel) == 0)
    {
        level = WARN;
    }
    else  if(strcasecmp("ERROR", logLevel) == 0)
    {
        level = ERROR;
    }
    else  if(strcasecmp("ALL", logLevel) == 0)
    {
        level = ALL;
    }
    else{
       return false;
    }
    char tempLogFileName[256] = {0};

    snprintf(tempLogFileName, 256, "%s%s", m_second_dir_name, logLevel);
    cout<< tempLogFileName <<endl;
    m_fp[level] = fopen(tempLogFileName, "a");

     for(int i = DEBUG; i <  LOG_LEVLE_MAX; i++)
     {
        if (m_fp[level] == NULL)
        {
            cout<<__FILE__<<"\t"<<__LINE__<<"\t"<<"文件打开失败"<<endl;
            return false;
        }
     }
    
    return true;
}

void Log::write_log(const char* logLevel, const char *format, ...)
{
    struct timeval now = {0, 0};
    gettimeofday(&now, NULL);
    time_t t = now.tv_sec;
    struct tm *sys_tm = localtime(&t);
    struct tm my_tm = *sys_tm;
    char s[16] = {0};

    LOG_LEVLE level;
    if (strcasecmp("DEBUG", logLevel) == 0)
    {
        level = DEBUG;
       strcpy(s, "[debug]:");
    }else if(strcasecmp("INFO", logLevel) == 0)
    {
        level = INFO;
        strcpy(s, "[info]:");
    }
    else  if(strcasecmp("WARN", logLevel) == 0)
    {
         level = WARN;
        strcpy(s, "[warn]:");
    }
    else  if(strcasecmp("ERROR", logLevel) == 0)
    {
        level = ERROR;
        strcpy(s, "[erro]:");
    }else{
       return ;
    }
    //写入一个log，对m_count++, m_split_lines最大行数
    m_mutex.lock();
    m_count++;

    //deng: 日志每天一个文件，或者达到最大行数也要分成两个文件
    if (m_today != my_tm.tm_mday || m_count % m_split_lines == 0) //everyday log
    {
        //deng: 关闭之前的文件，用新的日志文件存储日志
        for(int i = DEBUG; i <  LOG_LEVLE_MAX; i++)
        {
            fflush(m_fp[i]);
            fclose(m_fp[i]);
        }
       
        //在得到二级目录名
        memset(m_second_dir_name, '\0', sizeof(m_second_dir_name));
        if (m_today != my_tm.tm_mday)
        {
            snprintf(m_second_dir_name, 128, "%s/%d_%02d_%02d/", m_first_dir_name, my_tm.tm_year + 1900, my_tm.tm_mon + 1, my_tm.tm_mday);
            m_today = my_tm.tm_mday; // dneg: 更新最新的天数
            m_count = 0;//dneg: 过了一天了，行号清零
        }
        else
        {
            //deng: %lld=long long;
            snprintf(m_second_dir_name, 128, "%s/%d_%02d_%02d.%lld/", m_first_dir_name, my_tm.tm_year + 1900, my_tm.tm_mon + 1, my_tm.tm_mday, m_count / m_split_lines);
        }
         cout<< m_second_dir_name <<endl;
        //得到完整的目录名，创建目录。
        int ret = mkdir(m_second_dir_name, S_IRUSR|S_IWUSR); //dneg:所有者有读写权限
        // cout<< "mkdir2 ret == "<< ret << endl;
        openLogFile("DEBUG");
        openLogFile("INFO");
        openLogFile("WARN");
        openLogFile("ERROR");
        openLogFile("ALL");
    }
 
    m_mutex.unlock();

    va_list valst;
    va_start(valst, format);

    m_mutex.lock();

    //写入的具体时间内容格式
    int n = snprintf(m_buf, 48, "%d-%02d-%02d %02d:%02d:%02d.%06ld %s ",
                     my_tm.tm_year + 1900, my_tm.tm_mon + 1, my_tm.tm_mday,
                     my_tm.tm_hour, my_tm.tm_min, my_tm.tm_sec, now.tv_usec, s);
    
    //把具体的日志内容也放入到缓冲区中
    int m = vsnprintf(m_buf + n, sizeof(m_buf) - 1 - n, format, valst);
    m_buf[n + m] = '\n';
    m_buf[n + m + 1] = '\0';

    LogInfo single_log;
    single_log.content = m_buf;
    single_log.level = level;

    m_mutex.unlock();

    

    //dneg:异步
    if (m_is_async && !m_log_queue->full())
    {
        m_log_queue->push(single_log);
    }
    else//dneg:同步
    {
        m_mutex.lock();
        fputs(single_log.content.c_str(), m_fp[ALL]);
        fputs(single_log.content.c_str(), m_fp[single_log.level]);
        m_mutex.unlock();
    }

    va_end(valst);
}

void Log::flush(void)
{
    m_mutex.lock();
    //强制刷新写入流缓冲区
    for(int i = DEBUG; i <  LOG_LEVLE_MAX; i++)
        fflush(m_fp[i]);
    m_mutex.unlock();
}



void * Log::async_write_log()
{
    LogInfo single_log;
    //从阻塞队列中取出一个日志string，写入文件
    while (m_log_queue->pop(single_log))
    {
        m_mutex.lock();
        fputs(single_log.content.c_str(), m_fp[single_log.level]);
        fputs(single_log.content.c_str(), m_fp[ALL]);
        m_mutex.unlock();
    }
}