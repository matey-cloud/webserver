#include "httpconnection.h"

// 所有socket上的事件都被注册到同一个epoll内核事件中，所以设置成静态的
int HttpConnection::sm_epollfd = -1;
// 所有的客户数
int HttpConnection::sm_user_count = 0;

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
// 修改文件描述符，重置socket上的EPOLLONESHOT事件，以确保下一次可读时，EPOLLIN事件能被触发
void modifyfd(int epoll_fd, int fd, int ev){
    epoll_event event;
    event.data,fd = fd;
    event.events = ev | EPOLLIN | EPOLLONESHOT | EPOLLRDHUP;
    epoll_ctl(epoll_fd, EPOLL_CTL_MOD, fd, &event);
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
    init();
}
void HttpConnection::init(){

    m_read_index = 0;//初始化读缓冲区已读取数据的最后一个字节的索引
    m_checked_index = 0;//初始化当前正在分析的字符在读缓冲区中的位置
    m_start_line = 0;//初始化解析行的位置
    m_checked_state = CHECK_STATE_REQUESTLINE;//初始状态为检查请求行
    m_method = GET;                         //默认请求方式为GET
    m_url = NULL;//客户请求的目标文件的文件名
    m_version = NULL;//HTTP协议版本号，我们仅支持HTTP1.1
    m_host = NULL;//主机名
    m_content_len = 0;//HTTP请求的消息总长度
    m_is_keep_link = false;//默认不保持链接  Connection : keep-alive保持连接
    m_write_index = 0;//写缓冲区中待发送的字节数

    memset(m_read_buf, 0, READ_BUFFER_SIZE);
    memset(m_write_buf, 0, WRITE_BUFFER_SIZE);
    memset(m_reafile, 0, FILENAME_LEN);
}

void HttpConnection::closeConnection()
{
    if(m_socketfd != -1){
        sm_user_count--;
        m_socketfd = -1;
        close(m_socketfd);
    }
}

void HttpConnection::process()
{

}

bool HttpConnection::read()
{
    if(m_read_index >= READ_BUFFER_SIZE){
        //当前读缓冲区的索引超出读缓冲区的大小
        return false;
    }
    int read_num = 0;
    while(true){
        read_num = recv(m_socketfd, m_read_buf+m_read_index, READ_BUFFER_SIZE, 0);
        if(read_num == -1){
            if(errno == EAGAIN || errno == EWOULDBLOCK){
                //读缓冲区没有数据
            }
            break;
        } else if(read_num ==0) {
            //对方关闭连接
            return false;
        }
        m_read_index += read_num;
    }
    return true;
}

bool HttpConnection::write()
{

}

//主状态机，解析请求
HttpConnection::HTTP_CODE HttpConnection::processRead()
{
    LINE_STATUS line_status = LINE_OK;
    HTTP_CODE ret = NO_REQUEST;
    char *text = NULL;
    while(((m_checked_state == CHECK_STATE_CONTENT) && (line_status == LINE_OK))
           || ((line_status = parseLine()) == LINE_OK)){
        //获取一行数据
        text = getLine();
        m_start_line = m_checked_index;
        std::cout << "got 1 http line: " << text << std::endl;

        switch (m_checked_state) {
            case CHECK_STATE_REQUESTLINE:{
                ret = parseRequestLine(text);
                if(ret == BAD_REQUEST){
                    return BAD_REQUEST;
                }
                break;
            }
            case CHECK_STATE_HEADER:{
                ret = parseRequestHead(text);
                if(ret == BAD_REQUEST){
                    return BAD_REQUEST;
                } else if(ret == GET_REQUEST){
                    return doRequest();
                }
                break;
            }
            case CHECK_STATE_CONTENT:{
                ret = parseRequestContent(text);
                if(ret == GET_REQUEST){
                    return doRequest();
                }
                line_status = LINE_OPEN;
                break;
            }
            default:{
                return INTERNAL_ERROR;
            }
        }
    }
    return NO_REQUEST;
}

bool HttpConnection::processWrite(HTTP_CODE ret)
{

}


HttpConnection::LINE_STATUS HttpConnection::parseLine() //解析一行，判断依据\r\n
{
    char tmp;//读取到的字符,是否为\r或者\n
    for( ; m_checked_index < m_read_index; ++m_checked_index) {//检测索引不能大于已读取缓冲区的索引
        tmp = m_read_buf[m_checked_index];
        if(tmp == '\r'){
            if((m_checked_index+1) >= m_read_index){
                return LINE_OPEN;
            } else if(m_read_buf[m_checked_index + 1] == '\n'){
                m_read_buf[m_checked_index++] = '\0'; //索引先自增，返回旧的索引值，内容赋值为字符串结束标志'\0'，
                m_read_buf[m_checked_index++] = '\0';
                return LINE_OK;
            }
            return LINE_BAD;
        } else if (tmp == '\n'){
            if(m_checked_index > 1 && (m_read_buf[m_checked_index - 1] == '\r')){
                m_read_buf[m_checked_index - 1] = '\0';
                m_read_buf[m_checked_index++] = '\0';
                return LINE_OK;
            }
            return LINE_BAD;
        }
    }
    return LINE_OPEN;
}

//解析HTTP请求行，获得请求方法，目标URL,以及HTTP版本号
HttpConnection::HTTP_CODE HttpConnection::parseRequestLine(char *text)
{
    //GET /index.html HTTP/1.1
    m_url = strpbrk(text, " \t"); //判断第二个参数中的字符哪个在text中最先出现并赋值给tmp
    if(!m_url){
        return BAD_REQUEST;
    }

    *m_url++ = '\0';//GET\0/index.html HTTP/1.1
    char *method = text;
    if(strcasecmp(method, "GET") == 0){
        m_method = GET;
    }else{
        return BAD_REQUEST;
    }

    // /index.html HTTP/1.1
    m_version = strpbrk(m_url, " \t");
    if(!m_version){
        return BAD_REQUEST;
    }
    // /index.html\0HTTP/1.1
    *m_version++ = '\0';
    if(strcasecmp(m_version, "HTTP/1.1") != 0){
        return BAD_REQUEST;
    }

    // m_url = "/index.html" 或者 "http://192.168.110.129:10000/index.html"
    if(strncasecmp(m_url, "http://", 7) == 0){
        m_url += 7;
        m_url = strchr(m_url, '/'); // 在参数 str 所指向的字符串中搜索第一次出现字符 c（一个无符号字符）的位置
    }
    if(!m_url || m_url[0] != '/'){
        return BAD_REQUEST;
    }

    m_checked_state = CHECK_STATE_HEADER;  //检查状态变成检查头
    return NO_REQUEST;
}

HttpConnection::HTTP_CODE HttpConnection::parseRequestHead(char *text)
{
    //遇到空行，表示头部字段解析完毕
    if(text[0] == '\0'){
        // 如果HTTP请求有消息体，则还需要读取m_content_length字节的消息体，
        // 状态机转移到CHECK_STATE_CONTENT状态
        if(m_content_len != 0){
            m_checked_state = CHECK_STATE_CONTENT;
            return NO_REQUEST;
        }
        return GET_REQUEST; //否则说明我们已经得到了一个完整的HTTP请求
    } else if(strncasecmp(text, "Connection:", 11) == 0){
        //处理Connection 头部字段  Connection: keep-alive
        text += 11;
        text += strspn(text, "\t");
        if(strcasecmp(text, "keep-alive") == 0){
            m_is_keep_link = true;
        }
    } else if(strncasecmp(text, "Content-Length:", 15) == 0){
        //处理Content-Length头部字段
        text += 15;
        text += strspn( text, " \t");
        m_content_len = atol(text);
    } else if (strncasecmp(text, "Host:", 5) == 0){
        //处理Host头部字段
        text += 5;
        text += strspn(text," \t");
        m_host = text;
    } else {
        std::cout << "oop! unknow header " << text << std::endl;
    }
    return NO_REQUEST;
}

//没有真正解析HTTP请求的消息体，只是判断它是否被完整的读入了
HttpConnection::HTTP_CODE HttpConnection::parseRequestContent(char *text)
{
    if(m_read_index >= (m_content_len + m_checked_index)){
        text[m_content_len] = '\0';
        return GET_REQUEST;
    }
    return NO_REQUEST;
}

HttpConnection::HTTP_CODE HttpConnection::doRequest()
{

}




