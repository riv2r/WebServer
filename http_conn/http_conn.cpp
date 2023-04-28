#include "http_conn.h"

const char* html_root="/home/user/Desktop/WebServer/HTML";

int setnonblocking(int fd)
{
    int old_option=fcntl(fd,F_GETFL);
    int new_option=old_option|O_NONBLOCK;
    fcntl(fd,F_SETFL,new_option);
    return old_option;
}

void addfd(int epollfd,int fd,bool one_shot)
{
    epoll_event event;
    event.data.fd=fd;
    event.events=EPOLLIN|EPOLLET|EPOLLRDHUP;
    if(one_shot) event.events|=EPOLLONESHOT;
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
    event.events=ev|EPOLLET|EPOLLONESHOT|EPOLLRDHUP;
    epoll_ctl(epollfd,EPOLL_CTL_MOD,fd,&event);
}

int http_conn::h_user_count=0;
int http_conn::h_epollfd=-1;

void http_conn::init(int sockfd,const sockaddr_in& addr)
{
    h_sockfd=sockfd;
    h_address=addr;
    addfd(h_epollfd,sockfd,true);
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
                else if(http_code==GET_REQUEST) return do_request();//GET_REQUEST;
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

http_conn::HTTP_CODE http_conn::do_request()
{
    strcpy(h_file_path,html_root);
    int len=strlen(html_root);
    strncpy(h_file_path+len,h_url,FILE_PATH_LEN-len-1);
    if(stat(h_file_path,&h_file_stat)<0) return NO_RESOURCE;
    if(!(h_file_stat.st_mode & S_IROTH)) return FORBIDDEN_REQUEST;
    if(S_ISDIR(h_file_stat.st_mode)) return BAD_REQUEST;
    int fd=open(h_file_path,O_RDONLY);
    h_file_buf=(char*)mmap(0,h_file_stat.st_size,PROT_READ,MAP_PRIVATE,fd,0);
    close(fd);
    return FILE_REQUEST;
}

void http_conn::unmap()
{
    if(h_file_buf)
    {
        munmap(h_file_buf,h_file_stat.st_size);
        h_file_buf=0;
    }
}

bool http_conn::process_write(HTTP_CODE http_code)
{
    if(http_code==FILE_REQUEST)
    {
        int ret=0;
        int len=0;
        ret=snprintf(h_write_buf,WRITE_BUFFER_SIZE-1,"%s %s\r\n","HTTP/1.1","200 OK");
        len+=ret;
        ret=snprintf(h_write_buf+len,WRITE_BUFFER_SIZE-1-len,"Content-Length: %d\r\n",int(h_file_stat.st_size));
        len+=ret;
        ret=snprintf(h_write_buf+len,WRITE_BUFFER_SIZE-1-len,"%s","\r\n");

        struct iovec iv[2];
        iv[0].iov_base=h_write_buf;
        iv[0].iov_len=strlen(h_write_buf);
        iv[1].iov_base=h_file_buf;
        iv[1].iov_len=h_file_stat.st_size;
        ret=writev(h_sockfd,iv,2);
        unmap();
    }
    return true;
}

void http_conn::process()
{
    HTTP_CODE read_ret=process_read();
    if(read_ret==NO_REQUEST)
    {
        modfd(h_epollfd,h_sockfd,EPOLLIN);
        return; 
    }
    bool write_ret=process_write(read_ret);
    if(!write_ret) close_http_conn();
    modfd(h_epollfd,h_sockfd,EPOLLOUT);
}

/*
void user_message::process()
{
    process_read();
}
*/
