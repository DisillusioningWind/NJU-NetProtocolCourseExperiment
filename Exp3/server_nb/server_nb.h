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
#include <pthread.h>
#include <vector>
#include <map>
#include <string>
#include <string.h>

#define LISTENQ 1024
#define SERV_PORT 12345

#define ERR(exp, msg) if(exp) {error(1, errno, msg);}
#define STR(str) #str
#define CLS system("clear");

//创建监听套接字
int server_listen(int port);
//设置非阻塞
void server_set_nonblocking(int fd);
//服务器菜单
void server_show_menu();
//服务器子线程处理函数
void* server_thread(void* arg);

#pragma region 自定义类型

enum estate : uint8_t
{
    login = 0x00,
    game
};
//用户状态
struct state
{
    estate st;
    const char* to_string()
    {
        switch (st)
        {
        case login:
            return STR(login);
        case game:
            return STR(game);
        default:
            return STR(unknown);
        }
    }
};

//用户出拳
enum answer : uint8_t
{
    rock = 0x00,
    paper,
    scissors
};

//对局内用户信息
struct userinfo
{
    std::string name;
    bool rdy;
    answer ans;
    uint8_t score;
};

// 对局信息
struct gameinfo
{
    userinfo p1;
    userinfo p2;
    uint8_t round;

    void zero()
    {
        memset(this, 0, sizeof(gameinfo));
    }

    void end_round()
    {
        if (p1.ans == p2.ans) {}
        else if (p1.ans == rock && p2.ans == scissors) p1.score++;
        else if (p1.ans == paper && p2.ans == rock) p1.score++;
        else if (p1.ans == scissors && p2.ans == paper) p1.score++;
        else p2.score++;
        round++;
    }

    gameinfo() { zero(); }

    gameinfo(std::string name)
    {
        zero();
        p1.name = name;
    }

    void set_p2(std::string name)
    {
        p2.name = name;
    }

    void set_rdy(std::string name, bool rdy)
    {
        if (p1.name == name)
            p1.rdy = rdy;
        else if (p2.name == name)
            p2.rdy = rdy;
    }

    void set_ans(std::string name, answer ans)
    {
        if (p1.name == name)
            p1.ans = ans;
        else if (p2.name == name)
            p2.ans = ans;
    }
};

#pragma endregion

//用户列表
extern std::map<std::string, state> ulist;
//对局列表
extern std::map<int, gameinfo> glist;
//用户列表更新标志
extern bool uchanged;