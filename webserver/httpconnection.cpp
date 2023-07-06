#include "httpconnection.h"

// 所有socket上的事件都被注册到同一个epoll内核事件中，所以设置成静态的
int sm_epollfd = -1;
// 所有的客户数
int sm_user_count = 0;

//设置文件描述符非阻塞
void setNonBlock(int fd){
    int oldflag = fcntl(fd, F_GETFL);
    int newflag = oldflag | SOCK_NONBLOCK;
    fcntl(fd, F_SETFL, newflag);
}

//向epoll中添加需要监听的文件描述符
void addfd(int epoll_fd, int fd, bool oneshut){
    epoll_event event;
    event.data.fd = fd;
    event.events = EPOLLIN | EPOLLOUT;
    if(oneshut){
        event.events |= EPOLLONESHOT;  //防止同一个通信被不同的线程处理
    }
    epoll_ctl(epoll_fd, EPOLL_CTL_ADD, fd, &event);
    setNonBlock(fd);     //设置文件描述符非阻塞
}
//从epoll中移除监听的文件描述符
void removefd(int epoll_fd, int fd){
    epoll_ctl(epoll_fd, EPOLL_CTL_DEL, fd, NULL);
    close(fd);
}


HttpConnection::HttpConnection()
{

}

// 初始化连接,外部调用初始化套接字地址
void HttpConnection::init(int socketfd, const sockaddr_in &addr)
{
    m_socketfd = socketfd;
    memcpy(&m_addr, &addr, sizeof(addr));
    // 端口复用
    int reuse = 1;
    setsockopt(m_socketfd, SOL_SOCKET, SO_REUSEPORT, &reuse, sizeof(reuse));
    addfd(sm_epollfd, m_socketfd, true);
    sm_user_count++;

}

void HttpConnection::closeConnection()
{
    if(m_socketfd != -1){
        sm_user_count--;
        m_socketfd = -1;
        close(m_socketfd);
    }
}
