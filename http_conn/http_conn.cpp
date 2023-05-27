#include "http_conn.h"

const char* ok_200_title="OK";

const char* error_400_title="Bad Request";
const char* error_400_form="Your request has bad syntax or is inherently impossible to satisfy.\n";

const char* error_403_title="Forbidden";
const char* error_403_form="You do not have permission to get file from this server.\n";

const char* error_404_title="Not Found";
const char* error_404_form="The requested file was not found on this server.\n";

const char* error_500_title="Internal Error";
const char* error_500_form="There was an unusual problem serving the requested file.\n";


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

    memset(h_file_path,'\0',FILE_PATH_LEN);

    memset(h_write_buf,'\0',WRITE_BUFFER_SIZE);
    h_write_idx=0;
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

    if(strlen(h_url)==1) strcat(h_url,"home.html");

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
        //printf("unknown header: %s\n",text);
    }

    return NO_REQUEST;
}

http_conn::HTTP_CODE http_conn::parse_content(char* text)
{
    if((h_content_length+h_checked_idx)<=h_read_idx)
    {
        text[h_content_length]='\0';
        h_content=text;
        return GET_REQUEST;
    }
    return NO_REQUEST;
}

http_conn::HTTP_CODE http_conn::process_read()
{
    LINE_STATUS line_status=LINE_OK;
    HTTP_CODE http_code=NO_REQUEST;
    char* text=0;
    while((h_check_state==CHECK_STATE_CONTENT && line_status==LINE_OK) || (line_status=parse_line())==LINE_OK)
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
            case CHECK_STATE_CONTENT:
            {
                http_code=parse_content(text);
                if(http_code==GET_REQUEST) return do_request();
                line_status=LINE_OPEN;
                break;
            }
            default:
            {
                return INTERNAL_ERROR;
            }
        }
    }
    return NO_REQUEST;
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

bool http_conn::add_response(const char* format,...)
{
    if(h_write_idx>=WRITE_BUFFER_SIZE) return false;
    va_list arg_list;
    va_start(arg_list,format);
    int len=vsnprintf(h_write_buf+h_write_idx,WRITE_BUFFER_SIZE-1-h_write_idx,format,arg_list);
    if(len>=(WRITE_BUFFER_SIZE-1-h_write_idx))
    {
        va_end(arg_list);
        return false;
    }
    h_write_idx+=len;
    va_end(arg_list);
    return true;
}

bool http_conn::add_status_line(int status,const char* title)
{
    return add_response("%s %d %s\r\n","HTTP/1.1",status,title);
}

bool http_conn::add_headers(int content_length)
{
    bool flag1=add_content_length(content_length);
    bool flag2=add_linger();
    bool flag3=add_blank_line();
    return flag1 && flag2 && flag3;
}

bool http_conn::add_content_length(int content_length)
{
    return add_response("Content-Length: %d\r\n",content_length);
}

bool http_conn::add_linger()
{
    return add_response("Connection: %s\r\n",h_linger?"keep-alive":"close");
}

bool http_conn::add_blank_line()
{
    return add_response("%s","\r\n");
}

bool http_conn::add_content(const char* content)
{
    return add_response("%s",content);
}

bool http_conn::process_write(HTTP_CODE http_code)
{
    switch(http_code)
    {
        case INTERNAL_ERROR:
        {
            add_status_line(500,error_500_title);
            add_headers(strlen(error_500_form));
            if(!add_content(error_500_form)) return false;
            break;
        }
        case BAD_REQUEST:
        {
            add_status_line(400,error_400_title);
            add_headers(strlen(error_400_form));
            if(!add_content(error_400_form)) return false;
            break;
        }
        case NO_RESOURCE:
        {
            add_status_line(404,error_404_title);
            add_headers(strlen(error_404_form));
            if(!add_content(error_404_form)) return false;
            break;
        }
        case FORBIDDEN_REQUEST:
        {
            add_status_line(403,error_403_title);
            add_headers(strlen(error_403_form));
            if(!add_content(error_403_form)) return false;
            break;
        }
        case FILE_REQUEST:
        {
            add_status_line(200,ok_200_title);
            if(h_file_stat.st_size!=0)
            {
                add_headers(h_file_stat.st_size);
                h_iv[0].iov_base=h_write_buf;
                h_iv[0].iov_len=strlen(h_write_buf);
                h_iv[1].iov_base=h_file_buf;
                h_iv[1].iov_len=h_file_stat.st_size;
                h_iv_count=2;
                return true;
            }
        }
        default:return false;
    }
    h_iv[0].iov_base=h_write_buf;
    h_iv[0].iov_len=strlen(h_write_buf);
    h_iv_count=1;
    return true;
}

bool http_conn::write()
{
    int temp=0;
    int bytes_have_send=0;
    int bytes_to_send=h_write_idx;
    if(bytes_to_send==0)
    {
        modfd(h_epollfd,h_sockfd,EPOLLIN);
        init(h_sockfd,h_address);
        return true;
    }
    while(1)
    {
        temp=writev(h_sockfd,h_iv,h_iv_count);
        if(temp<=-1)
        {
            if(errno==EAGAIN)
            {
                modfd(h_epollfd,h_sockfd,EPOLLOUT);
                return true;
            }
            unmap();
            return false;
        }
        bytes_to_send-=temp;
        bytes_have_send+=temp;
        if(bytes_to_send<=bytes_have_send)
        {
            unmap();
            if(h_linger)
            {
                init(h_sockfd,h_address);
                modfd(h_epollfd,h_sockfd,EPOLLIN);
                return true;
            }
            else
            {
                modfd(h_epollfd,h_sockfd,EPOLLIN);
                return false;
            }
        }
    }
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
