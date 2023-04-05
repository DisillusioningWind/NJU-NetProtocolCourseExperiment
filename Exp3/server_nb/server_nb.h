#pragma once
#include <pthread.h>
#include "../common.h"

#define LISTENQ 1024

//创建监听套接字
int server_listen(int port);
//设置非阻塞
void server_set_nonblocking(int fd);
//服务器菜单
void server_show_menu();
//服务器子线程处理函数
void* server_thread(void* arg);
//子线程对收到包的处理
void server_handle(int res, clipkt &p, srvpkt &spkt);

#pragma region 自定义类型

enum class estate : uint8_t
{
    login = 0x00,
    game
};
//用户状态
struct state
{
    estate st;
    int gid;
    state() : st(estate::login) {}
    void join_game(int g) {st = estate::game; gid = g;}
    const char* to_string()
    {
        switch (st)
        {
        case estate::login:
            return STR(login);
        case estate::game:
            return STR(game);
        default:
            return STR(unknown);
        }
    }
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
        else if (p1.ans == answer::rock && p2.ans == answer::scissors) p1.score++;
        else if (p1.ans == answer::paper && p2.ans == answer::rock) p1.score++;
        else if (p1.ans == answer::scissors && p2.ans == answer::paper) p1.score++;
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
//共享资源读取线程数
extern int readcnt;
//总线程数
extern int threadcnt;
//共享资源读写锁
extern pthread_rwlock_t rwlock;