/**
 * @Author       : Yang Li
 * @Date         : 2023:07:05
 * @Description  : main.cpp
 *
 **/
#include <iostream>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/epoll.h>
#include <errno.h>
#include <exception>
#include <cassert>
#include <signal.h>

#include "locker.h"
#include "threadpool.h"
#include "threadpool.cpp"
#include "httpconnection.h"

#define MAX_FD 65536
#define MAX_EVENT_NUMBER 10000


extern void addfd(int epoll_fd, int fd, bool oneshut);  //向epoll中添加需要监听的文件描述符
extern void removefd(int epoll_fd, int fd);             //从epoll中移除监听的文件描述符
void addsig(int sig, void( handler )(int)){
    struct sigaction sa;
    memset( &sa, '\0', sizeof( sa));
    sa.sa_handler = handler;
    sigfillset( &sa.sa_mask );
    assert( sigaction( sig, &sa, NULL ) != -1 );
}

int main(int argc, char* argv[])
{
    if(argc <= 1){
        std::cout << "you should print: %s port_number" << basename(argv[0]) << std::endl;
        return 1;
    }

    int port = atoi(argv[1]); //端口号字符串转为整型
    addsig( SIGPIPE, SIG_IGN );

    ThreadPool<HttpConnection>* pool = NULL;    //创建线程池
    try{
        pool = new ThreadPool<HttpConnection>;
    } catch(...){
        return 1;
    }

    HttpConnection* users = new HttpConnection[MAX_FD]; //创建用户连接最大量

    int listen_sock = socket(AF_INET, SOCK_STREAM, 0); //主线程 监听套接字
    if(listen_sock == -1){
        perror("socket");
        exit(-1);
    }

    //端口复用
    int reuse = 1;
    setsockopt(listen_sock, SOL_SOCKET, SO_REUSEPORT, &reuse, sizeof(reuse));

    struct sockaddr_in ser_addr;
    ser_addr.sin_family = AF_INET;
    ser_addr.sin_port = htons(port);
    ser_addr.sin_addr.s_addr = INADDR_ANY;
//    inet_pton(AF_INET, "127.0.0.1", &(ser_addr.sin_addr.s_addr));
    int ret = bind(listen_sock, (struct sockaddr*)&ser_addr, sizeof(ser_addr)); //绑定套接字
    if(ret == -1){
        perror("bind");
        exit(-1);
    }

    ret = listen(listen_sock, 8);   //监听
    if(ret == -1){
        perror("listen");
        exit(-1);
    }

    epoll_event events[MAX_EVENT_NUMBER];   //创建事件数组
    int epollfd = epoll_create(10);         //创建epoll对象，参数无效但不能为0
    addfd(epollfd, listen_sock, false);     //不需要设置EPOLLONESHUT事件，因为只有主线程管理监听事件
    HttpConnection::sm_epollfd = epollfd;

    while(1){
        int num = epoll_wait(epollfd, events, MAX_EVENT_NUMBER, -1);  //阻塞,直到检测到 fd 数据发生变化
        if(num < 0 && (errno != EINTR)){
            std::cout << "epoll failure" << std::endl;
            break;
        }

        for(int i = 0; i < num; ++i){
            int sockfd = events[i].data.fd;
            if(sockfd == listen_sock){     //监听套接字有数据，说明有连接
                struct sockaddr_in client_addr; //储存用户的ip地址和端口号
                socklen_t len_clent_addr = sizeof(client_addr);
                int connfd = accept(listen_sock, (struct sockaddr*)&client_addr, &len_clent_addr);
                if(connfd < 0){
                    std::cout << "errno is " << errno << std::endl;
                    continue;
                }

                if(HttpConnection::sm_user_count >= MAX_FD){
                    std::cout << "user full" << std::endl;
                    close(connfd);
                    continue;
                }
                //可以正常工作,用已连接套接字作为该用户的索引,并初始化
                users[connfd].init(connfd, client_addr);      

            } else if(events[i].events & (EPOLLERR | EPOLLRDHUP | EPOLLHUP)){
                /* EPOLLRDHUP：对端关闭连接或对端关闭写半端
                 * EPOLLERR：发生错误
                 * EPOLLHUP：连接关闭
                 */
                users[sockfd].closeConnection();
            } else if(events[i].events & EPOLLIN){
                //读事件
                if(users[sockfd].read()){
                    //将用户的http请求添加的任务队列中
                    pool->append(users + sockfd);
//                    std::cout << "this" << std::endl;

                } else {
                    users[sockfd].closeConnection();
                }
            } else if(events[i].events & EPOLLOUT){
                if(!users[sockfd].write()){
                    users[sockfd].closeConnection();
                }
            }
        }
    }

    close(epollfd);
    close(listen_sock);
    delete[] users;
    delete pool;
    return 0;
}
