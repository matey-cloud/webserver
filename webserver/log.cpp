#include "log.h"

using namespace std;

Log::Log() {
    m_lineCount = 0;
    m_toDay = 0;
    m_isAsync = false;
    m_fp = nullptr;
    m_deque = nullptr;
    m_writeThread = nullptr;
}

Log::~Log() {
    if(m_writeThread && m_writeThread->joinable()) {
        while(!m_deque->empty()) {
            m_deque->flush();
        };
        m_deque->Close();
        m_writeThread->join();
    }
    if(m_fp) {
        lock_guard<mutex> locker(m_mtx);
        flush();
        fclose(m_fp);
    }
}
// 获取日志级别
int Log::GetLevel() {
    lock_guard<mutex> locker(m_mtx);
    return m_level;
}

//设置日志级别
void Log::SetLevel(int level) {
    lock_guard<mutex> locker(m_mtx);
    m_level = level;
}
//初始化日志记录器。根据参数设置日志级别、路径、后缀、最大队列容量。如果最大队列容量大于0，则启用异步写入模式，在新线程中刷新队列中的日志内容。
void Log::init(int level = 1, const char* path, const char* suffix,
               int maxQueueSize) {
    m_isOpen = true;
    m_level = level;
    if(maxQueueSize > 0) {
        m_isAsync = true;
        if(!m_deque) {
            unique_ptr<BlockDeque<std::string>> newDeque(new BlockDeque<std::string>);
            m_deque = move(newDeque);

            std::unique_ptr<std::thread> NewThread(new thread(FlushLogThread));
            m_writeThread = move(NewThread);
        }
    } else {
        m_isAsync = false;
    }

    m_lineCount = 0;

    time_t timer = time(nullptr);
    struct tm *sysTime = localtime(&timer);
    struct tm t = *sysTime;

    m_path = path;
    m_suffix = suffix;
    char fileName[LOG_NAME_LEN] = {0};
    snprintf(fileName, LOG_NAME_LEN - 1, "%s/%04d_%02d_%02d%s",
             m_path, t.tm_year + 1900, t.tm_mon + 1, t.tm_mday, m_suffix);
    m_toDay = t.tm_mday;

    {
        lock_guard<mutex> locker(m_mtx);

        m_buff.RetrieveAll();
        if(m_fp) {
            flush();
            fclose(m_fp);
        }

        m_fp = fopen(fileName, "a");
        if(m_fp == nullptr) {
            mkdir(m_path, 0777);
            m_fp = fopen(fileName, "a");
        }
        assert(m_fp != nullptr);
    }
}
//根据指定的日志级别和格式，将日志内容写入缓冲区
//根据当前日期和日志行数，可能创建新的日志文件
//如果是异步写入模式，将日志内容推入队列；否则直接将内容写入文件
void Log::write(int level, const char *format, ...) {
    struct timeval now = {0, 0};
    gettimeofday(&now, nullptr);
    time_t tSec = now.tv_sec;
    struct tm *sysTime = localtime(&tSec);
    struct tm t = *sysTime;
    va_list vaList;

    /* 日志日期 日志行数 */
    if (m_toDay != t.tm_mday || (m_lineCount && (m_lineCount  %  MAX_LINES == 0)))
    {
        unique_lock<mutex> locker(m_mtx);
        locker.unlock();

        char newFile[LOG_NAME_LEN];
        char tail[36] = {0};
        snprintf(tail, 36, "%04d_%02d_%02d", t.tm_year + 1900, t.tm_mon + 1, t.tm_mday);

        if (m_toDay != t.tm_mday)
        {
            snprintf(newFile, LOG_NAME_LEN - 72, "%s/%s%s", m_path, tail, m_suffix);
            m_toDay = t.tm_mday;
            m_lineCount = 0;
        }
        else {
            snprintf(newFile, LOG_NAME_LEN - 72, "%s/%s-%d%s", m_path, tail, (m_lineCount  / MAX_LINES), m_suffix);
        }

        locker.lock();
        flush();
        fclose(m_fp);
        m_fp = fopen(newFile, "a");
        assert(m_fp != nullptr);
    }
    else{
        unique_lock<mutex> locker(m_mtx);
        m_lineCount++;
        int n = snprintf(m_buff.BeginWrite(), 128, "%d-%02d-%02d %02d:%02d:%02d.%06ld ",
                         t.tm_year + 1900, t.tm_mon + 1, t.tm_mday,
                         t.tm_hour, t.tm_min, t.tm_sec, now.tv_usec);

        m_buff.HasWritten(n);
        AppendLogLevelTitle(level);

        va_start(vaList, format);
        int m = vsnprintf(buff_.BeginWrite(), buff_.WritableBytes(), format, vaList);
        va_end(vaList);

        m_buff.HasWritten(m);
        m_buff.Append("\n\0", 2);

        if(isAsync_ && deque_ && !deque_->full()) {
            deque_->push_back(m_buff.RetrieveAllToStr());
        } else {
            fputs(m_buff.Peek(), m_fp);
        }
        m_buff.RetrieveAll();
    }
}

//根据日志级别在缓冲区中添加相应的日志级别标题
void Log::AppendLogLevelTitle(int level) {
    switch(level) {
    case 0:
        m_buff.Append("[debug]: ", 9);
        break;
    case 1:
        m_buff.Append("[info] : ", 9);
        break;
    case 2:
        m_buff.Append("[warn] : ", 9);
        break;
    case 3:
        m_buff.Append("[error]: ", 9);
        break;
    default:
        m_buff.Append("[info] : ", 9);
        break;
    }
}
//刷新缓冲区，并将内容写入文件。如果是异步写入模式，则刷新队列中的日志内容。
void Log::flush() {
    if(m_isAsync) {
        m_deque->flush();
    }
    fflush(m_fp);
}
//异步写入模式下的写入线程函数，从队列中获取日志内容，并将其写入文件
void Log::AsyncWrite() {
    string str = "";
    while(m_deque->pop(str)) {
        lock_guard<mutex> locker(m_mtx);
        fputs(str.c_str(), m_fp);
    }
}

Log* Log::Instance() {
    static Log inst;
    return &inst;
}
//静态函数，用于启动异步写入模式下的写入线程
void Log::FlushLogThread() {
    Log::Instance()->AsyncWrite();
}
