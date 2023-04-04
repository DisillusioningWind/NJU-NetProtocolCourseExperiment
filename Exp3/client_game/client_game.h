#pragma once
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <error.h>
#include <sys/socket.h> //基本套接字定义
#include <sys/types.h>  //基本系统数据类型
#include <sys/time.h>   //select调用超时值结构体
#include <netinet/in.h> //sockaddr_in及其他网络定义
#include <arpa/inet.h>  //inet(3)函数
#include <fcntl.h>      //设置非阻塞
#include <unistd.h>
#include <signal.h>
//#include <pthread.h>
#include <vector>
#include <map>
#include <string>
#include <string.h>

#define SERV_IP "127.0.0.1"
#define SERV_PORT 12345

#define ERR(exp, msg) if(exp) {error(1, errno, msg);}
#define STR(str) #str
#define CLS system("clear");

//创建客户端套接字
int client_connect(const char* ip, int port);
//客户端菜单
void client_show_menu();
