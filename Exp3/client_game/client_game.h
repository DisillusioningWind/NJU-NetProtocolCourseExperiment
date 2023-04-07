#pragma once
#include "../common.h"

#define SERV_IP "127.0.0.1"

//创建客户端套接字
int client_connect(const char* ip, int port);
//客户端菜单
void client_show_menu();

void client_end_round();

struct player
{
    std::string name;
    answer ans;
    int score;
    void zero() {name.clear(); ans = answer::unknown; score = 0;}
    player() {zero();}
    player(std::string n, int s) : name(n), score(s) {}
};

struct fri
{
    std::string name;
    estate st;
};

extern int stage;
extern int res;
extern std::map<int, fri> flist;
extern int round;
extern int roundres;
extern player self;
extern player oppo;

extern srvpkt spkt;
extern clipkt cpkt;