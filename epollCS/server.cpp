#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/epoll.h>
#include <pthread.h>
#include <signal.h>

#include "../lock/locker.h"
#include "../threadpool/threadpool.h"
#include "../http_conn/http_conn.h"

#define MAX_FD 128
#define MAX_EVENT_NUM 1024

extern int addfd(int epollfd,int fd);
extern int removefd(int epollfd,int fd);

/*
int setnonblocking(int fd)
{
    int old_option=fcntl(fd,F_GETFL);
    int new_option=old_option|O_NONBLOCK;
    fcntl(fd,F_SETFL,new_option);
    return old_option;
}
*/

static bool stop=false;
// SIGTERM
static void handle_term(int sig)
{
    stop=true;
}

/*
void addfd(int epollfd,int fd)
{
    epoll_event event;
    event.data.fd=fd;
    event.events=EPOLLIN|EPOLLET;
    epoll_ctl(epollfd,EPOLL_CTL_ADD,fd,&event);
    setnonblocking(fd);
}
*/

int main(int argc, char* argv[])
{
    signal(SIGTERM,handle_term);

    if(argc<=2)
    {
        printf("usage: %s ip_address port_number\n",basename(argv[0]));
        return 1;
    }
    const char* ip=argv[1];
    int port=atoi(argv[2]);

    threadpool<http_conn>* pool=NULL;
    try
    {
        pool=new threadpool<http_conn>;
    }
    catch(...)
    {
        return 1;
    }

    http_conn* users=new http_conn[MAX_FD];
    assert(users);
    // int user_count=0;

    int ret=0;
    struct sockaddr_in address;
    bzero(&address,sizeof(address));
    address.sin_family=AF_INET;
    inet_pton(AF_INET,ip,&address.sin_addr);    
    address.sin_port=htons(port);

    int listenfd=socket(PF_INET,SOCK_STREAM,0);
    assert(listenfd>=0);

    ret=bind(listenfd,(struct sockaddr*)&address,sizeof(address));
    assert(ret!=-1);

    ret=listen(listenfd,5);
    assert(ret!=-1);

    epoll_event events[MAX_EVENT_NUM];
    int epollfd=epoll_create(5);
    assert(epollfd!=-1);
    addfd(epollfd,listenfd);
    http_conn::h_epollfd=epollfd;

    while(true)
    {
        int num=epoll_wait(epollfd,events,MAX_EVENT_NUM,-1);
        if(num<0)
        {
            printf("epoll failure\n");
            break;
        }

        for(int i=0;i<num;++i)
        {
            int sockfd=events[i].data.fd;
            if(sockfd==listenfd)
            {
                struct sockaddr_in client_address;
                socklen_t client_addrlength=sizeof(client_address);
                int connfd=accept(listenfd,(struct sockaddr*)&client_address,&client_addrlength);
                //addfd(epollfd,connfd);
                users[connfd].init(connfd,client_address);
            }
            else if(events[i].events & EPOLLIN)
            {
                if(users[sockfd].read()) pool->append(users+sockfd);
                else 
                {
                    users[sockfd].close_http_conn();
                }
                /*
                char buf[BUF_SIZE];
                memset(buf,'\0',BUF_SIZE);
                ret=recv(sockfd,buf,BUF_SIZE-1,0);
                if(ret<0) 
                {
                    if(errno==EAGAIN || errno==EWOULDBLOCK) close(sockfd);
                }
                else if(ret==0){}
                else
                {
                    if(strcmp(buf,"exit")!=0) printf("%s\n",buf);
                }
                */
            }
            else if(events[i].events & EPOLLOUT)
            {
                users[sockfd].close_http_conn();
            }
            else {}
        }
    }
    close(epollfd);
    close(listenfd);
    delete[] users;
    delete pool;
    return 0;
}
