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
#include <fcntl.h>
#include <string.h>

#include "listtimer.h"
#include "httpconnection.h"
#include "threadpool.cpp"

const static int FD_LIMIT = 65536;
const static int MAX_EVENT_NUMBER = 10000;
const static int TIMESLOT = 5;
static int pipefd[2];
static sort_timer_lst timer_lst;
static int epollfd = 0;

struct client{
    struct client_data userdata;
    HttpConnection user;
};

int setnonblocking( int fd )
{
    int old_option = fcntl( fd, F_GETFL );
    int new_option = old_option | O_NONBLOCK;
    fcntl( fd, F_SETFL, new_option );
    return old_option;
}

void addfd( int epollfd, int fd )
{
    epoll_event event;
    event.data.fd = fd;
    event.events = EPOLLIN | EPOLLET;
    epoll_ctl( epollfd, EPOLL_CTL_ADD, fd, &event );
    setnonblocking( fd );
}

void sig_handler(int sig){
    int save_errno = errno;
    int msg = sig;
    send(pipefd[1], (char*) &msg, 1, 0);
    errno = save_errno;
}
void addsig(int sig){
    struct sigaction sa;
    memset( &sa, '\0', sizeof( sa));
    sa.sa_handler = sig_handler;
    sa.sa_flags |= SA_RESTART;
    sigfillset(&sa.sa_mask);
    assert(sigaction( sig, &sa, NULL ) != -1);
}
void addsig(int sig, void( handler )(int)){
    struct sigaction sa;
    memset( &sa, '\0', sizeof( sa ) );
    sa.sa_handler = handler;
    sigfillset( &sa.sa_mask );
    assert( sigaction( sig, &sa, NULL ) != -1 );
}

void timer_handler(){

    //定时处理任务，实际上就是调用tick()函数
    timer_lst.tick();
    //因为一次 alarm 调用只会引起一次SIGALARM 信号，所以我们要重新定时，以不断触发 SIGALARM信号。
    alarm(TIMESLOT);
}

//定时器回调函数，它删除非活动连接socket上的注册事件，并关闭
void cb_func(client_data* user_data){
    epoll_ctl(epollfd, EPOLL_CTL_DEL, user_data->sockfd, 0);
    assert(user_data);
    close(user_data->sockfd);
    std::cout << "close fd " << user_data->sockfd << std::endl;
}

//int main(int argc, char* argv[])
int main()
{
    //    if(argc <= 1){
    //        std::cout << "you should print: %s port_number" << basename(argv[0]) << std::endl;
    //        return 1;
    //    }
    //int port = atoi(argv[1]); //端口号字符串转为整型

    int port = atoi("9999");
    addsig( SIGPIPE, SIG_IGN );

    ThreadPool<HttpConnection>* pool = NULL;
    try {
        pool = new ThreadPool<HttpConnection>;
    } catch( ... ) {
        return 1;
    }

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
    epollfd = epoll_create(10);         //创建epoll对象，参数无效但不能为0
    addfd(epollfd, listen_sock);
    HttpConnection::sm_epollfd = epollfd;
    //创建管道
    ret = socketpair(PF_UNIX, SOCK_STREAM, 0, pipefd);
    if(ret == -1){
        perror("socketpair");
        exit(-1);
    }
    setnonblocking(pipefd[1]);
    addfd(epollfd, pipefd[0]);

    // 设置信号处理函数
    addsig(SIGALRM);
    addsig(SIGTERM);
    bool stop_server = false;

    client* users = new client[FD_LIMIT];
    bool timeout = false;
    alarm(TIMESLOT);//5秒后产生SIGALARM信号

    while(!stop_server){
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
                if ( connfd < 0 ) {
                    printf( "errno is: %d\n", errno );
                    continue;
                }

                if( HttpConnection::sm_user_count >= FD_LIMIT ) {
                    close(connfd);
                    continue;
                }
                users[connfd].user.init( connfd, client_addr);

                //addfd(epollfd, connfd);
                users[connfd].userdata.address = client_addr;
                users[connfd].userdata.sockfd = connfd;

                util_timer* timer = new util_timer;
                timer->user_data = &users[connfd].userdata;
                timer->cb_func = cb_func;
                time_t cur = time(NULL);
                timer->expire = cur +  3 * TIMESLOT;
                users[connfd].userdata.timer = timer;
                timer_lst.add_timer(timer);

            } else if( events[i].events & ( EPOLLRDHUP | EPOLLHUP | EPOLLERR ) ) {

                users[sockfd].user.closeConnection();

            } else if((sockfd == pipefd[0]) && events[i].events & EPOLLIN){
                //处理信号
                int sig;
                char signals[1024];
                ret = recv(pipefd[0], signals, sizeof(signals), 0);
                if(ret == -1){
                    continue;
                } else if(ret == 0){
                    continue;
                } else {
                    for(int i =0; i < ret; ++i){
                        sig = (int)signals[i];
                        switch(sig){
                        case SIGALRM:
                        {
                            timeout = true;
                            break;
                        }
                        case SIGTERM:
                        {
                            stop_server = true;
                        }
                        }
                    }
                }
            } else if(events[i].events & EPOLLIN){

                memset(users[sockfd].userdata.buf, '\0', BUFFER_SIZE);
                //ret = recv(sockfd, users[sockfd].userdata.buf, BUFFER_SIZE - 1, 0);

                if(users[sockfd].user.read()){
                    pool->append(&users[sockfd].user);
                }


                std::cout << "get " << ret << " bytes of client data " << users[sockfd].userdata.buf << " from " << sockfd << std::endl;
                util_timer* timer = users[sockfd].userdata.timer;
                if(ret < 0){
                    //如果发生读错误，则关闭连接，并移除其对应的定时器
                    if(errno != EAGAIN){
                        cb_func(&users[sockfd].userdata);
                        users[sockfd].user.closeConnection();
                        if(timer){
                            timer_lst.del_timer(timer);
                        }
                    }
                } else if(ret == 0){
                    //如果对方已经关闭连接，则我们也关闭连接，并移除对应的定时器
                    cb_func(&users[sockfd].userdata);
                    if(timer){
                        timer_lst.del_timer(timer);
                    }
                } else {
                    //如果某个客户端上有数据可读，则我们要调整该连接对应的定时器，以延迟该连接被关闭的时间。
                    if(timer){
                        time_t cur = time(NULL);
                        timer->expire = cur + 3 * TIMESLOT;
                        std::cout << "adjust timer once" << std::endl;
                        timer_lst.adjust_timer(timer);
                    }
                }
            } else if( events[i].events & EPOLLOUT ) {

                if( !users[sockfd].user.write() ) {
                    users[sockfd].user.closeConnection();
                }
            }
        }
        if(timeout){
            timer_handler();
            timeout = false;
        }
    }

    close(listen_sock);
    close(pipefd[0]);
    close(pipefd[1]);
    delete[] users;
    delete pool;
    return 0;
}
