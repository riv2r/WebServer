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
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <sys/mman.h>
#include <stdarg.h>

class http_conn
{
public:
    static const int FILE_PATH_LEN=200;
    static const int READ_BUFFER_SIZE=2048;
    static const int WRITE_BUFFER_SIZE=1024;
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
                   CLOSED_CONNECTION,
                   NO_RESOURCE,
                   FILE_REQUEST};

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
    bool write();
private:
    HTTP_CODE process_read();
    LINE_STATUS parse_line();
    HTTP_CODE parse_requestline(char* text);
    HTTP_CODE parse_headers(char* text);
    HTTP_CODE do_request();
private:
    void unmap();
    bool process_write(HTTP_CODE http_code);
    bool add_response(const char* format,...);
    bool add_status_line(int status,const char* title);
    bool add_headers(int content_length);
    bool add_content_length(int content_length);
    bool add_linger();
    bool add_blank_line();
    bool add_content(const char* content);
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
    // html信息
    char h_file_path[FILE_PATH_LEN];
    char* h_file_buf;
    struct stat h_file_stat;
    struct iovec h_iv[2];
    int h_iv_count;
    // 写
    char h_write_buf[WRITE_BUFFER_SIZE];
    int h_write_idx;
};

#endif