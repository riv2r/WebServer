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

using namespace std;

#define BUF_SIZE 1024

static bool stop=false;
static void handle_term(int sig)
{
    stop=true;
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

    // 接受连接
    int connfd;
    char clientIP[INET_ADDRSTRLEN];
    struct sockaddr_in clientAddr;
    socklen_t clientAddrLen=sizeof(clientAddr);

    while(true)
    {
        connfd=accept(sockfd,(struct sockaddr*)&clientAddr,&clientAddrLen);
        if(connfd<0) cout<<"Error: accept"<<endl;
        inet_ntop(AF_INET,&clientAddr.sin_addr,clientIP,INET_ADDRSTRLEN);
        cout<<"...connect "<<clientIP<<":"<<clientAddr.sin_port<<endl;

        char buf[BUF_SIZE];
        
        while(true)
        {
            memset(buf,0,sizeof(buf));
            int ret=recv(connfd,buf,sizeof(buf),0);
            if(ret<=0) 
            {
                cout<<"...disconnect "<<clientIP<<":"<<clientAddr.sin_port<<endl;
                break;
            }
            buf[ret]='\0';
            if(strcmp(buf,"exit")==0)
            {
                cout<<"...disconnect "<<clientIP<<":"<<clientAddr.sin_port<<endl;
                break;
            }
            cout<<buf<<endl;
        }
        close(connfd);
    }
    close(sockfd);
    return 0;
}
