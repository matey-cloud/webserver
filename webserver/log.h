#ifndef LOG_H
#define LOG_H

#include <mutex>
#include <string>
#include <thread>
#include <sys/time.h>
#include <string.h>
#include <stdarg.h>           // vastart va_end
#include <assert.h>
#include <sys/stat.h>         //mkdir
#include "blockqueue.h"
#include "../buffer/buffer.h"

class Log {
public:
    //初始化日志记录器，设置日志级别、路径、后缀和最大队列容量等参数
    void init(int level, const char* path = "./log",
              const char* suffix =".log",
              int maxQueueCapacity = 1024);

    static Log* Instance();       //获取Log类的单例实例
    static void FlushLogThread();

    void write(int level, const char *format,...);  //根据指定的日志级别和格式，将日志内容写入缓冲区
    void flush();                   //刷新缓冲区，将内容写入磁盘文件
    //获取和设置日志级别
    int GetLevel();
    void SetLevel(int level);

    bool IsOpen() { return m_isOpen; }   //判断日志记录器是否已打开

private:
    Log();
    void AppendLogLevelTitle(int level);
    virtual ~Log();
    void AsyncWrite();

private:
    static const int LOG_PATH_LEN = 256;
    static const int LOG_NAME_LEN = 256;
    static const int MAX_LINES = 50000;

    int m_level;          //级别
    const char* m_path;   //路径
    const char* m_suffix; //后缀

    int m_MAX_LINES;
    int m_lineCount;
    int m_toDay;
    Buffer m_buff;
    bool m_isOpen;
    bool m_isAsync;

    FILE* m_fp;
    std::unique_ptr<BlockDeque<std::string>> m_deque;
    std::unique_ptr<std::thread> m_writeThread;
    std::mutex m_mtx;
};

#define LOG_BASE(level, format, ...) \
do {\
        Log* log = Log::Instance();\
        if (log->IsOpen() && log->GetLevel() <= level) {\
            log->write(level, format, ##__VA_ARGS__); \
            log->flush();\
    }\
} while(0);

#define LOG_DEBUG(format, ...) do {LOG_BASE(0, format, ##__VA_ARGS__)} while(0);
#define LOG_INFO(format, ...) do {LOG_BASE(1, format, ##__VA_ARGS__)} while(0);
#define LOG_WARN(format, ...) do {LOG_BASE(2, format, ##__VA_ARGS__)} while(0);
#define LOG_ERROR(format, ...) do {LOG_BASE(3, format, ##__VA_ARGS__)} while(0);

#endif //LOG_H
