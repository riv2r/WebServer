#include <iostream>
#include <cstdlib>
#include <cstdio>
#include <cerrno>
#include <cstring>
#include <csignal>

#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <unistd.h>
#include <arpa/inet.h>

#define BUF_SIZE 1024

static bool stop=false;
static void handle_term(int sig)
{
    stop=true;
}

int main(int argc,char const* argv[])
{
    signal(SIGTERM,handle_term);

    if(argc<=3)
    {
        std::cout<<"Usage: "<<argv[0]<<" ip_address port_number backlog"<<std::endl;
        return 1;
    }
    std::cout<<"This is Server"<<std::endl;

    // build server
    int serverfd=socket(PF_INET,SOCK_STREAM,0);
    if(serverfd==-1)
    {
        std::cout<<"Error: socket"<<std::endl;
        return 0;
    }

    // bind
    const char* ip=argv[1];
    int port=atoi(argv[2]);
    int backlog=atoi(argv[3]);

    struct sockaddr_in serverAddr;
    serverAddr.sin_family=AF_INET;
    serverAddr.sin_port=htons(port);
    inet_pton(AF_INET,ip,&serverAddr.sin_addr);
    
    if(bind(serverfd,(struct sockaddr*)&serverAddr,sizeof(serverAddr))==-1)
    {
        std::cout<<"Error: bind"<<std::endl;
        return 0;
    }

    // listen
    if(listen(serverfd,backlog)==-1)
    {
        std::cout<<"Error: listen"<<std::endl;
        return 0;
    }

    // accept
    int clientfd;
    char clientIP[INET_ADDRSTRLEN];
    struct sockaddr_in clientAddr;
    socklen_t clientAddrLen=sizeof(clientAddr);

    while (true)
    {
        std::cout<<"...listening"<<std::endl;
        clientfd=accept(serverfd,(struct sockaddr*)&clientAddr,&clientAddrLen);
        if(clientfd<0)
        {
            std::cout<<"Error: accept"<<std::endl;
        }
        inet_ntop(AF_INET,&clientAddr.sin_addr,clientIP,INET_ADDRSTRLEN);
        std::cout<<"...connect"<<clientIP<<":"<<ntohs(clientAddr.sin_port)<<std::endl;
        
        char buf[BUF_SIZE];
        while(true)
        {
            memset(buf,0,sizeof(buf));
            int ret=recv(clientfd,buf,sizeof(buf),0);
            buf[ret]='\0';
            if(strcmp(buf,"exit")==0)
            {
                std::cout<<"...disconnect"<<clientIP<<":"<<ntohs(clientAddr.sin_port)<<std::endl;
                break;
            }
            std::cout<<buf<<std::endl;
        }
        close(clientfd);
    }
    close(serverfd);
    return 0;
}