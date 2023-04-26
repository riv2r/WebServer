#include "http_conn.h"

int setnonblocking(int fd)
{
    int old_option=fcntl(fd,F_GETFL);
    int new_option=old_option|O_NONBLOCK;
    fcntl(fd,F_SETFL,new_option);
    return old_option;
}

void addfd(int epollfd,int fd)
{
    epoll_event event;
    event.data.fd=fd;
    event.events=EPOLLIN|EPOLLET|EPOLLRDHUP;
    epoll_ctl(epollfd,EPOLL_CTL_ADD,fd,&event);
    setnonblocking(fd);
}

void removefd(int epollfd,int fd)
{
    epoll_ctl(epollfd,EPOLL_CTL_DEL,fd,0);
    close(fd);
}

void modfd(int epollfd,int fd,int ev)
{
    epoll_event event;
    event.data.fd=fd;
    event.events=ev|EPOLLET|EPOLLRDHUP;
    epoll_ctl(epollfd,EPOLL_CTL_MOD,fd,&event);
}

int http_conn::h_user_count=0;
int http_conn::h_epollfd=-1;

void http_conn::init(int sockfd,const sockaddr_in& addr)
{
    h_sockfd=sockfd;
    h_address=addr;
    addfd(h_epollfd,sockfd);
    ++h_user_count;

    h_method=GET;
    h_url=0;
    h_version=0; 

    h_check_state=CHECK_STATE_REQUESTLINE;

    memset(h_read_buf,'\0',READ_BUFFER_SIZE);
    h_checked_idx=0;
    h_read_idx=0;
    h_start_idx=0;

    h_linger=false;
    h_content_length=0;
    h_host=0;
}

void http_conn::close_http_conn(bool real_close)
{
    if(real_close && h_sockfd!=-1)
    {
        removefd(h_epollfd,h_sockfd);
        h_sockfd=-1;
        --h_user_count;
    }
}

/*
非阻塞读
循环读取直至无数据可读或者对方关闭连接
*/
bool http_conn::read()
{
    if(h_read_idx>=READ_BUFFER_SIZE) return false;

    int ret=0;
    while(true)
    {
        ret=recv(h_sockfd,h_read_buf+h_read_idx,READ_BUFFER_SIZE-h_read_idx,0);
        if(ret<0)
        {
            if(errno==EAGAIN || errno==EWOULDBLOCK) break;
            return false;
        }
        else if(ret==0) return false;
        h_read_idx+=ret;
    }
    return true;
}

http_conn::LINE_STATUS http_conn::parse_line()
{
    char temp;
    for(;h_checked_idx<h_read_idx;++h_checked_idx)
    {
        temp=h_read_buf[h_checked_idx];
        if(temp=='\r')
        {
            if((h_checked_idx+1)==h_read_idx) return LINE_OPEN;
            else if(h_read_buf[h_checked_idx+1]=='\n')
            {
                h_read_buf[h_checked_idx++]='\0';
                h_read_buf[h_checked_idx++]='\0';
                return LINE_OK;
            }
            return LINE_BAD;
        }
        else if(temp=='\n')
        {
            if(h_checked_idx>1 && h_read_buf[h_checked_idx-1]=='\r')
            {
                h_read_buf[h_checked_idx-1]=='\0';
                h_read_buf[h_checked_idx++]=='\0';
                return LINE_OK;
            }
            return LINE_BAD;
        }
    }
    return LINE_OPEN;
}

http_conn::HTTP_CODE http_conn::parse_requestline(char* text)
{
    h_url=strpbrk(text," \t");
    if(!h_url) return BAD_REQUEST;
    *h_url++='\0';

    char* method=text;
    if(strcasecmp(method,"GET")==0) h_method=GET;
    else if(strcasecmp(method,"POST")==0) h_method=POST;
    else return BAD_REQUEST;

    h_url+=strspn(h_url," \t");
    h_version=strpbrk(h_url," \t");
    if(!h_version) return BAD_REQUEST;
    *h_version++='\0';
    h_version+=strspn(h_version," \t");

    if(strcasecmp(h_version,"HTTP/1.1")!=0) return BAD_REQUEST;

    if(strncasecmp(h_url,"http://",7)==0)
    {
        h_url+=7;
        h_url=strchr(h_url,'/');
    }

    if(!h_url || h_url[0]!='/') return BAD_REQUEST;

    h_check_state=CHECK_STATE_HEADER;
    return NO_REQUEST;
}

http_conn::HTTP_CODE http_conn::parse_headers(char* text)
{
    if(text[0]=='\0')
    {
        if(h_content_length!=0)
        {
            h_check_state=CHECK_STATE_CONTENT;
            return NO_REQUEST;
        }
        return GET_REQUEST;
    }
    else if(strncasecmp(text,"Connection:",11)==0)
    {
        text+=11;
        text+=strspn(text," \t");
        if(strcasecmp(text,"keep-alive")==0) h_linger=true;
    }
    else if(strncasecmp(text,"Content-Length:",15)==0)
    {
        text+=15;
        text+=strspn(text," \t");
        h_content_length=atol(text);
    }
    else if(strncasecmp(text,"Host:",5)==0)
    {
        text+=5;
        text+=strspn(text," \t");
        h_host=text;
    }
    else
    {
        printf("unknown header: %s\n",text);
    }

    return NO_REQUEST;
}

http_conn::HTTP_CODE http_conn::process_read()
{
    LINE_STATUS line_status=LINE_OK;
    HTTP_CODE http_code=NO_REQUEST;
    char* text=0;
    while((line_status=parse_line())==LINE_OK)
    {
        text=h_read_buf+h_start_idx;
        h_start_idx=h_checked_idx;
        switch(h_check_state)
        {
            case CHECK_STATE_REQUESTLINE:
            {
                http_code=parse_requestline(text);
                if(http_code==BAD_REQUEST) return BAD_REQUEST;
                break;
            }
            case CHECK_STATE_HEADER:
            {
                http_code=parse_headers(text);
                if(http_code==BAD_REQUEST) return BAD_REQUEST;
                else if(http_code==GET_REQUEST) return GET_REQUEST;
                break;
            }
            default:
            {
                return INTERNAL_ERROR;
            }
        }
    }
    if(line_status==LINE_OPEN) return NO_REQUEST;
    else return BAD_REQUEST;
}

void http_conn::process()
{
    HTTP_CODE ret=process_read();
    if(ret==GET_REQUEST)
    {
        printf("PARSE SUCCESSFULLY!\n"); 
    }
    modfd(h_epollfd,h_sockfd,EPOLLOUT);
}

/*
void user_message::process()
{
    process_read();
}
*/
