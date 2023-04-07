#pragma once
#include <pthread.h>
#include "../common.h"

#define LISTENQ 1024

//创建监听套接字
int server_listen(int port);
//设置非阻塞
void server_set_nonblocking(int fd);
//显示服务器菜单，如果有用户状态变化则更新
void server_show_menu(int cfd);
//服务器子线程处理函数
void* server_thread(void* arg);
//子线程对不同状态的处理
void server_handle(int cfd, int &stage, std::string& name, clipkt &cpkt, srvpkt &spkt);
//子线程根据收到的包设置不同的状态
int server_recv_state(int rt, clipkt &p);

#pragma region 自定义类型


//用户状态及对局号
struct state
{
    estate st;
    int gid;
    state() : st(estate::login),gid(-1) {}
    void join_game(int g) {st = estate::game; gid = g;}
    void leave_game() {st = estate::login; gid = -1;}
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
    bool rdy;
    bool refuse;
    bool quit;
    answer ans;
    uint8_t score;
};

// 对局信息
struct gameinfo
{
    std::map<std::string, userinfo> pl;
    std::map<std::string, answer> ans;
    std::map<std::string, uint8_t> read;
    uint8_t round;

    void zero() {memset(this, 0, sizeof(gameinfo));}

    gameinfo() { zero(); }
    gameinfo(std::string name)
    {
        zero();
        pl.insert({name, userinfo()});
        read.insert({name, 0});
    }

    void end_round()
    {
        auto it = pl.begin();
        std::string p1name = it->first;
        userinfo p1 = it->second;
        it++;
        std::string p2name = it->first;
        userinfo p2 = it->second;
        if(read[p1name] != 0 || read[p2name] != 0) return;
        if(p1.ans == answer::unknown || p2.ans == answer::unknown) return;
        else if (p1.ans == p2.ans) {}
        else if (p1.ans == answer::rock && p2.ans == answer::scissors) p1.score++;
        else if (p1.ans == answer::paper && p2.ans == answer::rock) p1.score++;
        else if (p1.ans == answer::scissors && p2.ans == answer::paper) p1.score++;
        else p2.score++;
        ans[get_p1_name()] = p1.ans;
        ans[get_p2_name()] = p2.ans;
        p1.ans = answer::unknown;
        p2.ans = answer::unknown;
        read[p1name]++;
        read[p2name]++;
    }

    void next_round()
    {
        round++;
        read[get_p1_name()] = 0;
        read[get_p2_name()] = 0;
    }

    userinfo& get_p1() {return pl.begin()->second;}
    userinfo& get_p2() {return (++pl.begin())->second;}
    userinfo& get_oppo(std::string name) {return name == get_p1_name() ? get_p2() : get_p1();}
    std::string get_p1_name() {return pl.begin()->first;}
    std::string get_p2_name() {return (++pl.begin())->first;}
    std::string get_oppo_name(std::string name) {return name == get_p1_name() ? get_p2_name() : get_p1_name();}

    void set_p2(std::string name) {pl.insert({ name, userinfo() }); read.insert({ name, 0 });}
    void set_rdy(std::string name, bool rdy) {pl[name].rdy = rdy;}
    void set_refuse(std::string name, bool refuse) {pl[name].refuse = refuse;}
    void set_quit(std::string name, bool quit) {pl[name].quit = quit;}
    bool is_rdy(std::string name) {return pl[name].rdy;}
    bool is_refuse(std::string name) {return pl[name].refuse;}
    bool is_quit(std::string name) {return pl[name].quit;}
    void set_ans(std::string name, answer ans) {pl[name].ans = ans;}
    answer get_ans(std::string name) {return pl[name].ans;}
};

#pragma endregion

//用户列表
extern std::map<std::string, state> ulist;
//对局列表
extern std::map<int, gameinfo> glist;
//用户列表更新标志
extern bool uchanged;
//共享资源读取线程数
extern std::map<int, bool> readcnt;
//共享资源读写锁
extern pthread_rwlock_t rwlock;