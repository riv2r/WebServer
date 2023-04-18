#ifndef USER_MESSAGE_H
#define USER_MESSAGE_H

#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>
#include <sys/stat.h>
#include <string.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <stdarg.h>
#include <errno.h>
#include <time.h>

class user_message
{
public:
    static const int READ_BUFFER_SIZE=2048;
public:
    user_message(){};
    ~user_message(){};
public:
    void init(int sockfd,const sockaddr_in& addr);
    void close_conn(bool real_close=true);
    void process();
    bool read();
private:
    void process_read();
public:
    static int u_epollfd;
    static int u_user_count;
private:
    int u_sockfd;
    sockaddr_in u_address;
    char read_buf[READ_BUFFER_SIZE];
};

#endif