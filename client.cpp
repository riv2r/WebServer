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

    int sockfd=socket(PF_INET,SOCK_STREAM,0);
    if(sockfd==-1)
    {
        cout<<"Error: socket"<<endl;
        return -1;
    }

    // 发起连接
    const char* ip=argv[1];
    int port=atoi(argv[2]);

    struct sockaddr_in serverAddr;
    serverAddr.sin_family=AF_INET;
    inet_pton(AF_INET,ip,&serverAddr.sin_addr);
    serverAddr.sin_port=htons(port);

    if(connect(sockfd,(struct sockaddr*)&serverAddr,sizeof(serverAddr))<0)
    {
        cout<<"Error: connect"<<endl;
        return -1;
    }

    cout<<"...connect"<<endl;
    char data[BUF_SIZE];

    while(true)
    {
        cin>>data;
        send(sockfd,data,strlen(data),0);
        if(strcmp(data,"exit")==0)
        {
            cout<<"...disconnect"<<endl;
            break;
        }
    }

    close(sockfd);
    return 0;
}
