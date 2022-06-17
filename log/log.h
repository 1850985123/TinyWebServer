#ifndef LOG_H
#define LOG_H

#include <stdio.h>
#include <iostream>
#include <string>
#include <stdarg.h>
#include <pthread.h>
#include "block_queue.h"

using namespace std;




class Log
{
public:
    //C++11以后,使用局部变量懒汉不用加锁
    static Log *get_instance()
    {
        static Log instance;
        return &instance;
    }

    static void *flush_log_thread(void *args)
    {
        Log::get_instance()->async_write_log();
    }
    //可选择的参数有日志文件、日志缓冲区大小、最大行数以及最长日志条队列
    bool init(const char *dir_name = "./MyLog", int close_log = 0,  int split_lines = 5000000, int max_queue_size = 0);
    void write_log(const char* logLevel, const char *format, ...);
    void flush(void);

private:
    Log();
    virtual ~Log();
    void *async_write_log();
private:
    bool openLogFile(const char * logLevel);
    enum LOG_LEVLE{
        DEBUG = 0,
        INFO,
        WARN,
        ERROR,
        ALL,
        LOG_LEVLE_MAX
    };
    struct LogInfo{
        LOG_LEVLE level;
        string content;
    };

    char m_first_dir_name[128]; //根目录路径名
    char m_second_dir_name[128]; //二级目录路径名

    int m_split_lines;  //日志最大行数
    long long m_count;  //日志行数记录
    int m_today;        //因为按天分类,记录当前时间是那一天
    FILE *m_fp[LOG_LEVLE_MAX];         //打开log的文件指针
    char m_buf[2048];
    block_queue<LogInfo> *m_log_queue; //阻塞队列
    bool m_is_async;                  //是否同步标志位
    locker m_mutex;
    int m_close_log; //关闭日志

    char **m_log_string;
};

#define LOG_STRING_LEVEL_COUNTS  {"DEBUG", "INFO", "WARN", "ERROR", "DNEG"}

#define LOG_DEBUG(format, ...) if(0 == m_close_log) {Log::get_instance()->write_log("DEBUG", format, ##__VA_ARGS__); Log::get_instance()->flush();}
#define LOG_INFO(format, ...) if(0 == m_close_log) {Log::get_instance()->write_log("INFO", format, ##__VA_ARGS__); Log::get_instance()->flush();}
#define LOG_WARN(format, ...) if(0 == m_close_log) {Log::get_instance()->write_log("WARN", format, ##__VA_ARGS__); Log::get_instance()->flush();}
#define LOG_ERROR(format, ...) if(0 == m_close_log) {Log::get_instance()->write_log("ERROR", format, ##__VA_ARGS__); Log::get_instance()->flush();}


// #define LOG_DENG(format, ...) if(0 == m_close_log) {Log::get_instance()->write_log("DNEG", format, ##__VA_ARGS__); Log::get_instance()->flush();}

// #define LOG_DEBUG(format, ...)
// #define LOG_INFO(format, ...) 
// #define LOG_WARN(format, ...) 
// #define LOG_ERROR(format, ...)


#endif
