#include "user_message.h"

int setnonblocking(int fd)
{
    int old_option=fcntl(fd,F_GETFL);
    int new_option=old_option|O_NONBLOCK;
    fcntl(fd,F_SETFL,new_option);
    return old_option;
}

void addfd(int epollfd,int fd)
{
    epoll_event event;
    event.data.fd=fd;
    event.events=EPOLLIN|EPOLLET;
    epoll_ctl(epollfd,EPOLL_CTL_ADD,fd,&event);
    setnonblocking(fd);
}

void removefd(int epollfd,int fd)
{
    epoll_ctl(epollfd,EPOLL_CTL_DEL,fd,0);
    close(fd);
}

int user_message::u_user_count=0;
int user_message::u_epollfd=-1;

void user_message::init(int sockfd,const sockaddr_in& addr)
{
    u_sockfd=sockfd;
    u_address=addr;
    addfd(u_epollfd,sockfd);
    ++u_user_count;
}

void user_message::close_conn(bool real_close)
{
    if(real_close && u_sockfd!=-1)
    {
        removefd(u_epollfd,u_sockfd);
        u_sockfd=-1;
        --u_user_count;
    }
}

bool user_message::read()
{
    memset(read_buf,'\0',READ_BUFFER_SIZE);
    int ret=0;
    ret=recv(u_sockfd,read_buf,READ_BUFFER_SIZE-1,0);
    if(ret<0)
    {
        if(errno==EAGAIN || errno==EWOULDBLOCK) return true;
    }
    else if(ret==0) return false;
    return true;
}

void user_message::process_read()
{
    if(strcmp(read_buf,"\0")!=0)
    {
        char str_1[]="message from ";
        char clientIP[INET_ADDRSTRLEN];
        inet_ntop(AF_INET,&u_address.sin_addr,clientIP,INET_ADDRSTRLEN);
        char* str_2=clientIP;
        char str_3[]=":";
        strcat(strcat(str_1,str_2),str_3);
        printf("%s%d ",str_1,u_address.sin_port);
        time_t curtime;
        time(&curtime);
        char* str_4=ctime(&curtime);
        strcat(str_4,read_buf);
        printf("%s\n",str_4);
    }
}

void user_message::process()
{
    process_read();
}