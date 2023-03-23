#include <iostream>
#include <csignal>
#include <sys/socket.h>
#include <arpa/inet.h>

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
    int serverfd=socket(PF_INET,SOCK_STREAM,0);
    if(serverfd==-1)
    {
        cout<<"Error: socket"<<endl;
        return -1;
    }

    // 绑定ip port等属性
    const char* ip=argv[1];
    int port=atoi(argv[2]);

    struct sockaddr_in serverAddr;
    serverAddr.sin_family=AF_INET;
    inet_pton(AF_INET,ip,&serverAddr.sin_addr);    
    serverAddr.sin_port=htons(port);

    if(bind(serverfd,(struct sockaddr*)&serverAddr,sizeof(serverAddr))==-1)
    {
        cout<<"Error: bind"<<endl;
        return -1;
    }

    // 监听
    if(listen(serverfd,5)==-1)
    {
        cout<<"Error: listen"<<endl;
        return -1;
    }

    // 接受连接

    return 0;
}
