#ifndef MYSQL_CONN_POOL_H
#define MYSQL_CONN_POOL_H

#include<cstdio>
#include<list>
#include<mysql/mysql.h>
#include<cstring>
#include<cstdlib>
#include<pthread.h>
#include<iostream>
#include<string>
#include "../lock/locker.h"

using namespace std;

class mysql_conn_pool
{
private:
    mysql_conn_pool();
    ~mysql_conn_pool();

    int h_mxConn;
    int h_curConn;
    int h_freeConn;
    locker lock;
    list<MYSQL*> connList;
    sem reserve;
public:
    string h_url;
    string h_port;
    string h_user;
    string h_password;
    string h_databaseName;
public:
    MYSQL* getConn();
    bool releaseConn(MYSQL* conn);
    void destroyPool();
    int getFreeConn();

    static mysql_conn_pool* getInstance();

    void init(string url="localhost",int port=3306,string user="root",string pwd="root",string dbName="webserverdb",int mxConn=8);
};

class connRAII
{
private:
    MYSQL* h_connRAII;
    mysql_conn_pool* h_poolRAII;
public:
    connRAII(MYSQL** conn,mysql_conn_pool* connPool);
    ~connRAII();
};

#endif
