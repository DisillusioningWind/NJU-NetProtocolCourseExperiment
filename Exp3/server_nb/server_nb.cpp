#include "server_nb.h"

//全局变量

std::map<std::string, state> ulist;
std::map<int, gameinfo> glist;
bool uchanged = false;
std::map<int, bool> readcnt;
pthread_rwlock_t rwlock;

//仅该文件内可见

int server_listen(int port)
{
    //创建套接字
    int listenfd;
    listenfd = socket(AF_INET, SOCK_STREAM, 0);
    //设置非阻塞
    //server_set_nonblocking(listenfd);
    //填充套接字地址
    struct sockaddr_in server_addr;
    bzero(&server_addr, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port = htons(port);
    //设置端口复用
    int on = 1;
    setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));
    //绑定
    int rt1 = bind(listenfd, (struct sockaddr *) &server_addr, sizeof(server_addr));
    ERR(rt1 < 0, "bind failed ")
    //监听
    int rt2 = listen(listenfd, LISTENQ);
    ERR(rt2 < 0, "listen failed ")
    //忽略SIGPIPE信号
    signal(SIGPIPE, SIG_IGN);

    return listenfd;
}

void server_set_nonblocking(int fd) {
    fcntl(fd, F_SETFL, O_NONBLOCK);
}

void server_show_menu(int cfd)
{
    int i = 0, g = 0;
    std::map<std::string, state>::iterator uit;
    pthread_rwlock_rdlock(&rwlock);
    if(uchanged && readcnt[cfd] == false)
    {
        readcnt[cfd] = true;
        int readsize = std::count_if(readcnt.begin(), readcnt.end(), [](std::pair<int, bool> p){return p.second == true;});
        if(readsize == readcnt.size())
        {
            uchanged = false;
            for(auto& p: readcnt) p.second = false;
        }
        CLS
        printf("**********online user list********************game info***********\n");
        uit = ulist.begin();gameinfo gi;
        for (;uit != ulist.end() || g < glist.size();)
        {
            if (uit != ulist.end())
            {
                printf("*%02d. name:%-8s state:%-10s", i + 1, uit->first.c_str(), uit->second.to_string());
                uit++;
                i++;
            }
            else
            {
                printf("*                         ");//27个空格
            }
            if (g < glist.size() && (gi = glist[g]).round != 0)
            {
                printf("*%02d. player1:%-8s player2:%-8s*\n", g + 1, gi.get_p1_name().c_str(), gi.get_p2_name().c_str());
                g++;
            }
            else
            {
                printf("*                             *\n");//31个空格
            }
        }
        printf("******************************************************************\n");
    }
    pthread_rwlock_unlock(&rwlock);
}

void *server_thread(void *arg)
{
    int cfd = *(int *)arg, stage = 0, rt = 0;
    fd_set rset, allset;
    timeval rtv, tv;
    clipkt cpkt;
    srvpkt spkt;
    std::string name;
    FD_ZERO(&allset);
    FD_SET(cfd, &allset);
    tv.tv_sec = 0;
    tv.tv_usec = 500000;//0.5s
    readcnt[cfd] = false;

    while(true)
    {
        rset = allset;
        rtv = tv;
        cpkt.zero();
        spkt.zero();
        int rc = select(cfd + 1, &rset, NULL, NULL, &rtv);
        ERR(rc < 0, "select failed ")
        else if(rc == 0) {stage = 1;}
        else if(FD_ISSET(cfd, &rset))
        {
            rt = recv(cfd, &cpkt, sizeof(cpkt), 0);
            stage = server_recv_state(rt, cpkt);
        }
        //根据stage的值进行不同的处理
        server_handle(cfd, stage, name, cpkt, spkt);
    }
    close(cfd);
    return NULL;
}

void server_handle(int cfd, int &stage, std::string& name, clipkt &cpkt, srvpkt &spkt)
{
    switch (stage)
    {
    //超时
    case 1:
    {
        if(name == "") return;
        //更新用户列表
        int i = 0;
        int gid = ulist[name].gid;
        pthread_rwlock_wrlock(&rwlock);
        if(uchanged && readcnt[cfd] == false)
        {
            readcnt[cfd] = true;
            int readsize = std::count_if(readcnt.begin(), readcnt.end(), [](std::pair<int, bool> p){return p.second == true;});
            if(readsize == readcnt.size())
            {
                uchanged = false;
                for(auto& p: readcnt) p.second = false;
            }
            auto uit = ulist.begin();
            for (;uit != ulist.end() && i < USER_NUM; uit++)
            {
                if(uit->first == name) continue;
                spkt.u[i].n = uit->first;
                spkt.u[i].st = uit->second.st;
                i++;
            }
            spkt.type = srvtype::userinfo;
            spkt.res_num = (uint8_t)i;
            send(cfd, &spkt, sizeof(spkt), 0);
            spkt.zero();
        }   
        if (gid != -1)
        {
            std::string oppo = glist[gid].get_oppo_name(name);
            //有对战邀请
            if(glist[gid].is_rdy(oppo) && !glist[gid].is_rdy(name))
            {
                spkt.type = srvtype::gamerequest;
                spkt.u[0].n = oppo;
                send(cfd, &spkt, sizeof(spkt), 0);
                spkt.zero();
            }
            //对手答应
            else if(glist[gid].is_rdy(oppo) && glist[gid].is_rdy(name))
            {
                glist[gid].set_rdy(name, false);
                glist[gid].set_rdy(oppo, false);
                spkt.type = srvtype::gamestart;
                spkt.u[0].n = oppo;
                spkt.round = 1;
                uchanged = true;
                send(cfd, &spkt, sizeof(spkt), 0);
                spkt.zero();
            }
            //对手拒绝
            else if(glist[gid].is_refuse(oppo))
            {
                glist.erase(gid);
                ulist[name].gid = -1;
                ulist[oppo].gid = -1;
                spkt.type = srvtype::gamerefuse;
                spkt.u[0].n = oppo;
                send(cfd, &spkt, sizeof(spkt), 0);
                spkt.zero();
            }
            //对手退出
            else if(glist[gid].is_quit(oppo))
            {
                glist.erase(gid);
                ulist[name].leave_game();
                ulist[oppo].leave_game();
                uchanged = true;
                spkt.type = srvtype::gamequit;
                spkt.u[0].n = oppo;
                send(cfd, &spkt, sizeof(spkt), 0);
                spkt.zero();
            }
            //对手出拳
            else if(glist[gid].round != 0)
            {
                glist[gid].end_round(name);
                if(glist[gid].read[name] == 1)
                {
                    spkt.type = srvtype::gameanswer;
                    spkt.u[0].n = oppo;
                    spkt.ans = glist[gid].ans[oppo];
                    spkt.score = glist[gid].get_oppo(oppo).score;
                    spkt.round = glist[gid].round;
                    send(cfd, &spkt, sizeof(spkt), 0);
                    spkt.zero();
                    glist[gid].read[name]++;
                }
                if(glist[gid].read[name] == 2 && glist[gid].read[oppo] == 2)
                {
                    glist[gid].next_round();
                    if(glist[gid].get_p1().score >= 2 || glist[gid].get_p2().score >= 2)
                    {
                        glist.erase(gid);
                        ulist[name].leave_game();
                        ulist[oppo].leave_game();
                        uchanged = true;
                    }
                }
            }
        }
        pthread_rwlock_unlock(&rwlock);
        break;
    }
    //客户端退出
    case 2:
    {
        //从游戏列表和用户列表中删除
        pthread_rwlock_wrlock(&rwlock);
        int gid = ulist[name].gid;
        if(gid != -1)
        {
            std::string oppo = glist[gid].get_oppo_name(name);
            glist.erase(gid);
            ulist[oppo].leave_game();
        }
        ulist.erase(name);    
        uchanged = true;
        readcnt.erase(cfd);
        pthread_rwlock_unlock(&rwlock);
        close(cfd);
        pthread_exit(NULL);
        break;
    }
    //客户端登录
    case 3:
    {
        name = cpkt.n.to_string();
        pthread_rwlock_wrlock(&rwlock);
        if (ulist.find(name) == ulist.end())
        {
            // 登录成功
            ulist[name] = state();
            uchanged = true;
            pthread_rwlock_unlock(&rwlock);
            spkt.type = srvtype::loginok;
        }
        else
        {
            // 登录失败
            pthread_rwlock_unlock(&rwlock);
            name.clear();
            spkt.type = srvtype::loginfail;
        }
        break;
    }
    //客户端发送邀请
    case 4:
    {
        std::string oppo = cpkt.n.to_string();
        pthread_rwlock_wrlock(&rwlock);
        if (ulist.find(oppo) != ulist.end() && ulist[oppo].st == estate::login && oppo != name)
        {
            //添加对局信息
            int gid = 0;
            if (glist.size() != 0)
            {
                auto git = glist.end();
                git--;
                gid = git->first + 1;
            }
            glist.insert({gid, gameinfo(name)});
            glist[gid].set_p2(oppo);
            glist[gid].set_rdy(name, true);
            ulist[name].gid = gid;
            ulist[oppo].gid = gid;
            pthread_rwlock_unlock(&rwlock);
        }
        else
        {
            // 请求失败
            pthread_rwlock_unlock(&rwlock);
            spkt.type = srvtype::gamerefuse;
            spkt.u[0].n = namepkt(oppo);
        }
        break;
    }
    //客户端接受邀请
    case 5:
    {
        std::string oppo = cpkt.n.to_string();
        pthread_rwlock_wrlock(&rwlock);
        int gid = ulist[name].gid;
        if (glist.find(gid) != glist.end())
        {
            //接受邀请
            glist[gid].set_rdy(name, true);
            if(glist[gid].is_rdy(oppo))
            {
                glist[gid].round = 1;
                ulist[name].join_game(gid);
                ulist[oppo].join_game(gid);
            }
            pthread_rwlock_unlock(&rwlock);
            spkt.type = srvtype::gamestart;
            spkt.u[0].n = namepkt(oppo);
            spkt.round = 1;
        }
        else
        {
            //接受失败
            pthread_rwlock_unlock(&rwlock);
            spkt.type = srvtype::gamerefuse;
            spkt.u[0].n = namepkt(name);
        }
        break;
    }
    //客户端拒绝邀请
    case 6:
    {
        std::string oppo = cpkt.n.to_string();
        pthread_rwlock_wrlock(&rwlock);
        int gid = ulist[name].gid;
        if (glist.find(gid) != glist.end())
        {
            //拒绝邀请
            glist[gid].set_refuse(name, true);
        }
        pthread_rwlock_unlock(&rwlock);
        break;
    }
    //客户端退出游戏
    case 7:
    {
        pthread_rwlock_wrlock(&rwlock);
        int gid = ulist[name].gid;
        if (glist.find(gid) != glist.end())
        {
            //退出游戏
            glist[gid].set_quit(name, true);
        }
        pthread_rwlock_unlock(&rwlock);
        break;
    }
    //客户端发送答案
    case 8:
    {
        pthread_rwlock_wrlock(&rwlock);
        int gid = ulist[name].gid;
        if (glist.find(gid) != glist.end())
        {
            //发送答案
            glist[gid].set_ans(name, cpkt.ans);
        }
        pthread_rwlock_unlock(&rwlock);
        break;
    }
    }
    if(spkt.type != srvtype::nosend)
    {
        send(cfd, &spkt, sizeof(spkt), 0);
    }
}

int server_recv_state(int rt, clipkt &p)
{
    if(rt == 0)
    {
        return 2;
    }
    else if(rt > 0)
    {
        switch(p.type)
        {
        case clitype::login:
        {
            return 3;
        }
        case clitype::gamerequest:
        {
            return 4;
        }
        case clitype::gameaccept:
        {
            return 5;
        }
        case clitype::gamerefuse:
        {
            return 6;
        }
        case clitype::gamequit:
        {
            return 7;
        }
        case clitype::gameanswer:
        {
            return 8;
        }
        }
    }
    else
    {
        return -1;
    }
    return 0;
}
