#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <cassert>
#include <cstdio>
#include <unistd.h>
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <cstdlib>
#include <poll.h>
#include <iostream>

#define USER_LIMIT 5
#define BUFFER_SIZE 64
#define FD_LIMIT 65535

struct client_data
{
    sockaddr_in address;
    char* write_buf;
    char buf[BUFFER_SIZE];
};

int setnonblocking(int fd)
{
    int old_option=fcntl(fd,F_GETFL);
    int new_option=old_option | O_NONBLOCK;
    fcntl(fd,F_SETFL,new_option);
    return old_option;
}

int main(int argc,char* argv[])
{
    if(argc<=2)
    {
        std::cout<<"usage: "<<argv[0]<<" ip_address port_number"<<std::endl;
        return 1;
    }

    const char* ip=argv[1];
    int port=atoi(argv[2]);

    int ret=0;

    struct sockaddr_in address;
    address.sin_family=AF_INET;
    inet_pton(AF_INET,ip,&address.sin_addr);
    address.sin_port=htons(port);
    //build socket
    int listenfd=socket(PF_INET,SOCK_STREAM,0);
    assert(listenfd>=0);
    //bind
    ret=bind(listenfd,(struct sockaddr*)&address,sizeof(address));
    assert(ret!=-1);
    //listen
    ret=listen(listenfd,5);
    assert(ret!=-1);

    client_data* users=new client_data[FD_LIMIT];
    pollfd fds[USER_LIMIT+1];

    int user_counter=0;
    for(int i=1;i<=USER_LIMIT;++i)
    {
        fds[i].fd=-1;
        fds[i].events=0;
    }
    fds[0].fd=listenfd;
    fds[0].events=POLLIN | POLLERR;
    fds[0].revents=0;

    while(1)
    {
        //ready fds_t
        ret=poll(fds,user_counter+1,-1);
        if(ret<0)
        {
            std::cout<<"poll failure"<<std::endl;
            break;
        }

        for(int i=0;i<user_counter+1;++i)
        {
            if((fds[i].fd==listenfd) && (fds[i].revents & POLLIN))
            {
                struct sockaddr_in client_address;
                socklen_t client_addrlength=sizeof(client_address);
                int connfd=accept(listenfd,(struct sockaddr*)&client_address,&client_addrlength);
                if(connfd<0)
                {
                    std::cout<<"errno is:"<<errno<<std::endl;
                    continue;
                }
                if(user_counter>=USER_LIMIT)
                {
                    const char* info="too many users";
                    std::cout<<info<<std::endl;
                    send(connfd,info,strlen(info),0);
                    close(connfd);
                    continue;
                }
                ++user_counter;
                users[connfd].address=client_address;
                // nonblock
                setnonblocking(connfd);
                fds[user_counter].fd=connfd;
                fds[user_counter].events=POLLIN|POLLRDHUP|POLLERR;
                fds[user_counter].revents=0;
                std::cout<<"come a new user, now have "<<user_counter<<" user(s)"<<std::endl;
            }
            else if(fds[i].revents & POLLERR)
            {
                std::cout<<"get an error from "<<fds[i].fd<<std::endl;
                char errors[100];
                memset(errors,'\0',100);
                socklen_t length=sizeof(errors);
                if(getsockopt(fds[i].fd,SOL_SOCKET,SO_ERROR,&errors,&length)<0) std::cout<<"get socket option failed"<<std::endl;
                continue;
            }
            else if(fds[i].revents & POLLRDHUP)
            {
                users[fds[i].fd]=users[fds[user_counter].fd];
                close(fds[i].fd);
                fds[i]=fds[user_counter];
                //still poll ith client
                --i;
                --user_counter;
                std::cout<<"a client left"<<std::endl;
            }
            else if(fds[i].revents & POLLIN)
            {
                int connfd=fds[i].fd;
                memset(users[connfd].buf,'\0',BUFFER_SIZE);
                ret=recv(connfd,users[connfd].buf,BUFFER_SIZE-1,0);
                std::cout<<"get "<<ret<<" bytes of client data: "<<users[connfd].buf<<" from "<<connfd<<std::endl;
                if(ret<0)
                {
                    //read process wrong
                    if(errno!=EAGAIN)
                    {
                        close(connfd);
                        fds[i]=fds[user_counter];
                        --i;
                        --user_counter;
                    }
                }
                else if(ret==0){}
                else
                {
                    for(int j=1;j<=user_counter;++j)
                    {
                        if(fds[j].fd==connfd) continue;
                        fds[j].events|=~POLLIN;
                        fds[j].events|=POLLOUT;
                        users[fds[j].fd].write_buf=users[connfd].buf;
                    }
                }
            }
            else if(fds[i].revents & POLLOUT)
            {
                int connfd=fds[i].fd;
                if(!users[connfd].write_buf) continue;
                ret=send(connfd,users[connfd].write_buf,strlen(users[connfd].write_buf),0);
                users[connfd].write_buf=NULL;
                fds[i].events|=~POLLOUT;
                fds[i].events=POLLIN;
            }
        }
    }
    delete []users;
    close(listenfd);
    return 0;
}
