#include <stdio.h>
#include <stdlib.h>
#include <string.h>
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
#include <pthread.h>

#define LISTENQ 1024
#define SERV_PORT 12345

//创建监听套接字
int server_listen(int port);
//设置非阻塞
void server_set_nonblocking(int fd);
//服务器子线程处理函数
void* server_thread(void* arg);