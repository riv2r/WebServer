#include "mysql_conn_pool.h"

mysql_conn_pool* mysql_conn_pool::getInstance()
{
    static mysql_conn_pool connPool;
    return &connPool;
}

mysql_conn_pool::mysql_conn_pool()
{
    h_curConn=0;
    h_freeConn=0;
}

mysql_conn_pool::~mysql_conn_pool()
{
    destroyPool();
}

void mysql_conn_pool::init(string url,int port,string user,string pwd,string dbName,int mxConn)
{
    h_url=url;
    h_port=port;
    h_user=user;
    h_password=pwd;
    h_databaseName=dbName;

    for(int i=0;i<mxConn;++i)
    {
        MYSQL* conn=NULL;
        conn=mysql_init(conn);

        if(conn==NULL) exit(1);
        conn=mysql_real_connect(conn,url.c_str(),user.c_str(),pwd.c_str(),dbName.c_str(),port,NULL,0);
        if(conn==NULL) exit(1);
        connList.push_back(conn);
        ++h_freeConn;
    }

    reserve=sem(h_freeConn);
    h_mxConn=h_freeConn;
}

MYSQL* mysql_conn_pool::getConn()
{
    MYSQL* conn=NULL;
    if(connList.size()==0) return NULL;
    reserve.wait();
    lock.lock();

    conn=connList.front();
    connList.pop_front();

    --h_freeConn;
    ++h_curConn;

    lock.unlock();
    return conn;
}

bool mysql_conn_pool::releaseConn(MYSQL* conn)
{
    if(conn==NULL) return false;
    lock.lock();

    connList.push_back(conn);
    ++h_freeConn;
    --h_curConn;

    lock.unlock();

    reserve.post();
    return true;
}

void mysql_conn_pool::destroyPool()
{
    lock.lock();
    if(connList.size()>0)
    {
        list<MYSQL*>::iterator itr;
        for(itr=connList.begin();itr!=connList.end();++itr)
        {
            MYSQL* conn=*itr;
            mysql_close(conn);
        }
        h_curConn=0;
        h_freeConn=0;

        connList.clear();
    }
    lock.unlock();
}

int mysql_conn_pool::getFreeConn()
{
    return h_freeConn;
}

connRAII::connRAII(MYSQL** conn,mysql_conn_pool* connPool)
{
    *conn=connPool->getConn();
    h_connRAII=*conn;
    h_poolRAII=connPool;
}

connRAII::~connRAII()
{
    h_poolRAII->releaseConn(h_connRAII);
}