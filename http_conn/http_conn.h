#ifndef HTTP_CONN_H
#define HTTP_CONN_H

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <sys/epoll.h>

class http_conn
{
public:
    static const int READ_BUFFER_SIZE=2048;
    enum CHECK_STATE{CHECK_STATE_REQUESTLINE=0,
                     CHECK_STATE_HEADER,
                     CHECK_STATE_CONTENT};

    enum LINE_STATUS{LINE_OK=0,
                     LINE_BAD,
                     LINE_OPEN};

    enum HTTP_CODE{NO_REQUEST,
                   GET_REQUEST,
                   BAD_REQUEST,
                   FORBIDDEN_REQUEST,
                   INTERNAL_ERROR,
                   CLOSED_CONNECTION};

    enum METHOD{GET=0,
                POST,
                HEAD,
                PUT,
                DELETE,
                TRACE,
                OPTIONS,
                CONNECT,
                PATCH};
public:
    http_conn(){};
    ~http_conn(){};
public:
    void init(int sockfd,const sockaddr_in& addr);
    void close_http_conn(bool real_close=true);
    void process();
    bool read();
private:
    HTTP_CODE process_read();
    LINE_STATUS parse_line();
    HTTP_CODE parse_requestline(char* text);
    HTTP_CODE parse_headers(char* text);
public:
    static int h_epollfd;
    static int h_user_count;
private:
    // 客户端信息
    int h_sockfd;
    sockaddr_in h_address;
    // HTTP请求报文 请求行信息
    METHOD h_method;
    char* h_url;
    char* h_version;
    // 主状态机状态
    CHECK_STATE h_check_state;
    // 读
    char h_read_buf[READ_BUFFER_SIZE];
    int h_checked_idx;
    int h_read_idx;
    int h_start_idx;
    // HTTP请求报文 请求头部信息
    bool h_linger;
    int h_content_length;
    char* h_host;
};

#endif