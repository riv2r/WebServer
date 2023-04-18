#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <signal.h>

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
        printf("usage: %s ip_address port_number\n",basename(argv[0]));
        return 1;
    }

    const char* ip=argv[1];
    int port=atoi(argv[2]);

    struct sockaddr_in server_address;
    bzero(&server_address,sizeof(server_address));
    server_address.sin_family=AF_INET;
    inet_pton(AF_INET,ip,&server_address.sin_addr);
    server_address.sin_port=htons(port);

    int sockfd=socket(PF_INET,SOCK_STREAM,0);
    assert(sockfd>=0);

    if(connect(sockfd,(struct sockaddr*)&server_address,sizeof(server_address))<0)
    {
        printf("Connection failed\n");
    }

    char data[BUF_SIZE];

    while(true)
    {
        scanf("%s",data);
        send(sockfd,data,strlen(data),0);
        if(strcmp(data,"exit")==0)
        {
            printf("disconnect\n");
            break;
        }
    }

    close(sockfd);
    return 0;
}
