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

#define SERV_PORT 12345

#define ERR(exp, msg) if(exp) {error(1, errno, msg);}
#define STR(str) #str
#define CLS system("clear");

#pragma pack(1)

//用户名
struct name
{
    char val[15] = { 0 };
    name() {}
    name(const char* s) {strncpy(val, s, 15);}
    name(const std::string& s) {strncpy(val, s.c_str(), 15);}
    name& operator=(const char* s) {strncpy(val, s, 15); return *this;}
    std::string to_string() {return std::string(val);}
};
//用户出拳
enum class answer : uint8_t
{
    rock = 0x00,
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
};
#pragma endregion
#pragma region srvpkt
//服务器发包类型
enum class srvtype : uint8_t
{
    loginok = 0x00,
    loginfail,
    gamerequest,
    gamewait,
    gamerefuse,
    gamestart,
    gameanswer,
    gamequit,
    userinfo
};
//服务器数据包，大小为155字节
struct srvpkt
{
    srvtype type;
    uint8_t res_num;
    uint8_t round;
    uint8_t score;
    answer ans;
    //以上占用5字节
    name n[10];
    void zero() {memset(this, 0, sizeof(srvpkt));}
    srvpkt() {zero();}
};
#pragma endregion

#pragma pack()