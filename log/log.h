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
    int getLogLevel(const char* logLevel);
    bool openLogFile(int  logLevel);
    bool creatDirAndFile(const char *first_dir_name, const char *second_dir_name);
private:
    struct LogInfo{
        int level;
        string content;
    };
    char m_first_dir_name[128]; //根目录路径名
    char m_second_dir_name[128]; //二级目录路径名

    int m_split_lines;  //日志最大行数
    long long m_count;  //日志行数记录
    int m_today;        //因为按天分类,记录当前时间是那一天
    FILE **m_fp;         //打开log的文件指针
    char m_buf[2048];   //日志内容缓冲区

    block_queue<LogInfo> *m_log_queue; //阻塞队列
    bool m_is_async;                  //是否同步标志位
    locker m_mutex;
    int m_close_log; //关闭日志

    int m_logLevelCounts; // 日志级别的数量
    char **m_logLevelStrings;  //用来保存日志级别字符串
};


#pragma GCC diagnostic ignored "-Wwrite-strings" //屏蔽一些过时的警告
#define LOG_STRING_LEVEL_COUNTS  {"ALL", "DEBUG", "INFO", "WARN", "ERROR", "DNEG", "GENG"}   //日志越靠前级别越低
#define LOG_FIRST_DIR_NAME "./mySeverLog"  //日志的根目录名字

#define LOG_DEBUG(format, ...)  {Log::get_instance()->write_log("DEBUG", format, ##__VA_ARGS__);}
#define LOG_INFO(format, ...)   {Log::get_instance()->write_log("INFO", format, ##__VA_ARGS__); }
#define LOG_WARN(format, ...)   {Log::get_instance()->write_log("WARN", format, ##__VA_ARGS__); }
#define LOG_ERROR(format, ...)  {Log::get_instance()->write_log("ERROR", format, ##__VA_ARGS__);}
#define LOG_DENG(format, ...)   {Log::get_instance()->write_log("DNEG", format, ##__VA_ARGS__);}
#define LOG_GENG(format, ...)   {Log::get_instance()->write_log("GENG", format, ##__VA_ARGS__);}

// #define LOG_DEBUG(format, ...)
// #define LOG_INFO(format, ...) 
// #define LOG_WARN(format, ...) 
// #define LOG_ERROR(format, ...)


#endif
