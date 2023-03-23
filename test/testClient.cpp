#include <iostream>
#include <cstdio>
#include <cstdlib>
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

int main(int argc, char* argv[])
{
    signal(SIGTERM,handle_term);
    if(argc<=2)
    {
        std::cout<<"Usage: "<<argv[0]<<" ip_address port_number"<<std::endl;
        return 1;
    }

    int clientfd=socket(PF_INET,SOCK_STREAM,0);
    if(clientfd==-1)
    {
        std::cout<<"Error: socket"<<std::endl;
        return 0;
    }

    // connect
    const char* ip=argv[1];
    int port=atoi(argv[2]);

    struct sockaddr_in serverAddr;
    serverAddr.sin_family=AF_INET;
    serverAddr.sin_port=htons(port);
    inet_pton(AF_INET,ip,&serverAddr.sin_addr);

    if(connect(clientfd,(struct sockaddr*)&serverAddr,sizeof(serverAddr))<0)
    {
        std::cout<<"Error: connect"<<std::endl;
        return 0;
    }

    std::cout<<"...connect"<<std::endl;
    char data[BUF_SIZE];
    char buf[BUF_SIZE];
    while(true)
    {
        std::cin>>data;
        send(clientfd,data,strlen(data),0);
        if(strcmp(data,"exit")==0)
        {
            std::cout<<"...disconnect"<<std::endl;
            break;
        }
        memset(buf,0,sizeof(buf));
    }
    close(clientfd);

    return 0;
}
