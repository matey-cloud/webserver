#ifndef HTTPCONNECTION_H
#define HTTPCONNECTION_H
/**
 * @Author       : Yang Li
 * @Date         : 2023:07:06
 * @Description  :
 *
 **/
#include <arpa/inet.h>
#include <sys/epoll.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

class HttpConnection
{
public:
    static int sm_epollfd;
    static int sm_user_count;

    HttpConnection();
    HttpConnection(HttpConnection&) = delete;
    HttpConnection(HttpConnection&&) = delete;
    ~HttpConnection();

    void init(int socketfd, const sockaddr_in &addr);  //初始化成员变量
    void closeConnection();                            //关闭连接

private:
    int m_socketfd;
    struct sockaddr_in m_addr;
};

#endif // HTTPCONNECTION_H
