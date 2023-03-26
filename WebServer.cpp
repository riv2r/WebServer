#include <iostream>
// memset
#include <cstring>
#include <csignal>
#include <sys/socket.h>
#include <sys/types.h>
// sockaddr_in
#include <arpa/inet.h>
// close(fd)
#include <unistd.h>
#include <sys/epoll.h>
#include <fcntl.h>

using namespace std;

#define MAX_EVENT_NUM 1024
#define TCP_BUF_SIZE 1024

// 设置为非阻塞
int setnonblocking(int fd)
{
    int old_option=fcntl(fd,F_GETFL);
    int new_option=old_option|O_NONBLOCK;
    fcntl(fd,F_SETFL,new_option);
    return old_option;
}

static bool stop=false;
static void handle_term(int sig)
{
    stop=true;
}

void addfd(int epollfd,int fd)
{
    epoll_event event;
    event.data.fd=fd;
    event.events=EPOLLIN|EPOLLET;
    epoll_ctl(epollfd,EPOLL_CTL_ADD,fd,&event);
    setnonblocking(fd);
}

int main(int argc, char const *argv[])
{
    // 中断信号回调函数
    signal(SIGTERM,handle_term);

    if(argc<=2)
    {
        cout<<"Usage: "<<argv[0]<<" ip_address port_number"<<endl;
        return -1;
    }

    // 建立服务端套接字
    int sockfd=socket(PF_INET,SOCK_STREAM,0);
    if(sockfd==-1)
    {
        cout<<"Error: socket"<<endl;
        return -1;
    }

    // 绑定ip port等属性
    const char* ip=argv[1];
    int port=atoi(argv[2]);

    struct sockaddr_in sockAddr;
    sockAddr.sin_family=AF_INET;
    inet_pton(AF_INET,ip,&sockAddr.sin_addr);    
    sockAddr.sin_port=htons(port);

    if(bind(sockfd,(struct sockaddr*)&sockAddr,sizeof(sockAddr))==-1)
    {
        cout<<"Error: bind"<<endl;
        return -1;
    }

    // 监听
    if(listen(sockfd,5)==-1)
    {
        cout<<"Error: listen"<<endl;
        return -1;
    }

    cout<<"...Listening"<<endl;

    // I/O复用 epoll
    epoll_event events[MAX_EVENT_NUM];
    int epollfd=epoll_create(5);
    if(epollfd==-1)
    {
        cout<<"Error: epoll"<<endl;
        return -1;
    }
    addfd(epollfd,sockfd);


    while(true)
    {
        int num=epoll_wait(epollfd,events,MAX_EVENT_NUM,-1);
        if(num<0)
        {
            cout<<"epoll failure"<<endl;
            break;
        }

        for(int i=0;i<num;++i)
        {
            int tempfd=events[i].data.fd;
            if(tempfd==sockfd)
            {
                // 接受连接
                struct sockaddr_in clientAddr;
                socklen_t clientAddrLen=sizeof(clientAddr);
                int connfd=accept(sockfd,(struct sockaddr*)&clientAddr,&clientAddrLen);
                
                if(connfd<0) cout<<"Error: accept"<<endl;
                // 获取客户端信息
                char clientIP[INET_ADDRSTRLEN];
                inet_ntop(AF_INET,&clientAddr.sin_addr,clientIP,INET_ADDRSTRLEN);
                cout<<"...connect "<<clientIP<<":"<<clientAddr.sin_port<<endl;
                
                addfd(epollfd,connfd);
            }
            else if(events[i].events & EPOLLIN)
            {
                char buf[TCP_BUF_SIZE];
        
                while(true)
                {
                    memset(buf,'\0',TCP_BUF_SIZE);
                    int ret=recv(tempfd,buf,TCP_BUF_SIZE-1,0);
                    if(ret<=0) 
                    {
                        cout<<"...disconnect"<<endl;
                        close(tempfd);
                        break;
                    }
                    else
                    {
                        cout<<buf<<endl;
                        // 避免阻塞
                        break;
                    }
                }
            }
        }
    }
    close(sockfd);
    return 0;
}
