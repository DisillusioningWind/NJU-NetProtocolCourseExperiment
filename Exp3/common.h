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
#include <vector>
#include <map>
#include <string>
#include <string.h>
#include <algorithm>

#define SERV_PORT 12345
#define USER_NUM 10

#define ERR(exp, msg) if(exp) {error(1, errno, msg);}
#define STR(str) #str
#define CLS system("clear");

#pragma pack(1)

//用户名
typedef struct name
{
    char val[15] = { 0 };
    name() {}
    name(const char* s) {strncpy(val, s, 15);}
    name(const std::string& s) {strncpy(val, s.c_str(), 15);}
    name& operator=(const char* s) {strncpy(val, s, 15); return *this;}
    std::string to_string() {return std::string(val);}
} namepkt;
//用户状态
enum class estate : uint8_t
{
    login = 0x00,
    game
};
//用户
struct user
{
    name n;
    estate st;
};
// 用户出拳
enum class answer : uint8_t {
    unknown = 0x00,
    rock,
    paper,
    scissors
};

#pragma region clipkt
//客户端发包类型
enum class clitype : uint8_t
{
    login = 0x00,
    gamerequest,
    gameaccept,
    gamerefuse,
    gamequit,
    gameanswer
};
//客户端数据包，大小为18字节
struct clipkt
{
    clitype type;
    uint8_t round;
    answer ans;
    name n;
    void zero() {memset(this, 0, sizeof(clipkt));}
};
#pragma endregion
#pragma region srvpkt
//服务器发包类型
enum class srvtype : uint8_t
{
    nosend = 0x00,
    loginok,
    loginfail,
    gamerequest,
    gamerefuse,
    gamestart,
    gameanswer,
    gamequit,
    userinfo
};
//服务器数据包，大小为165字节
struct srvpkt
{
    srvtype type;
    uint8_t round;
    answer ans;
    uint8_t score;
    uint8_t res_num;
    //以上占用5字节
    user u[USER_NUM];
    void zero() {memset(this, 0, sizeof(srvpkt));}
    srvpkt() {zero();}
};
#pragma endregion

#pragma pack()