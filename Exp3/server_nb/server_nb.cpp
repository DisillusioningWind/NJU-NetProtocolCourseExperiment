#include "server_nb.h"

//全局变量

std::map<std::string, state> ulist;
std::map<int, gameinfo> glist;
bool uchanged = false;
int readcnt = 0;
int threadcnt = 1;
pthread_rwlock_t rwlock;

//仅该文件内可见
//线程局部存储
pthread_key_t key;

int server_listen(int port)
{
    //创建套接字
    int listenfd;
    listenfd = socket(AF_INET, SOCK_STREAM, 0);
    //设置非阻塞
    server_set_nonblocking(listenfd);
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

void server_show_menu()
{
    int i = 0, g = 0;
    std::map<std::string, state>::iterator uit;
    pthread_rwlock_rdlock(&rwlock);
    if(uchanged)
    {
        readcnt++;
        if(readcnt == threadcnt)
        {
            uchanged = false;
            readcnt = 0;
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
    FD_ZERO(&allset);
    FD_SET(cfd, &allset);
    tv.tv_sec = 0;
    tv.tv_usec = 500000;//0.5s

    while(true)
    {
        rset = allset;
        rtv = tv;
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
        server_handle(cfd, stage, cpkt, spkt);
    }
    close(cfd);
    //线程结束
    pthread_rwlock_wrlock(&rwlock);
    threadcnt--;
    pthread_rwlock_unlock(&rwlock);
    return NULL;
}

void server_handle(int cfd, int &stage, clipkt &cpkt, srvpkt &spkt)
{
    switch (stage)
    {
    //超时
    case 1:
    {
        //to do 
        break;
    }
    //客户端退出
    case 2:
    {
        std::string n = *(std::string *)pthread_getspecific(key);
        ERR(n == "", "client not login ")
        //从游戏列表和用户列表中删除
        pthread_rwlock_wrlock(&rwlock);
        glist.erase(ulist[n].gid);
        ulist.erase(n);
        uchanged = true;
        threadcnt--;
        pthread_rwlock_unlock(&rwlock);
        close(cfd);
        pthread_exit(NULL);
    }
    //客户端登录
    case 3:
    {
        std::string n = cpkt.n.to_string();
        pthread_rwlock_wrlock(&rwlock);
        if (ulist.find(n) == ulist.end())
        {
            // 登录成功
            ulist[n] = state();
            uchanged = true;
            pthread_rwlock_unlock(&rwlock);
            pthread_setspecific(key, (void *)&n);
            spkt.type = srvtype::loginok;
        }
        else
        {
            // 登录失败
            pthread_rwlock_unlock(&rwlock);
            spkt.type = srvtype::loginfail;
        }
        break;
    }
    //客户端发送邀请
    case 4:
    {
        std::string oppo = cpkt.n.to_string();
        std::string n = *(std::string *)pthread_getspecific(key);
        ERR(n == "", "client not login ")
        pthread_rwlock_wrlock(&rwlock);
        if (ulist.find(oppo) != ulist.end() && ulist[oppo].st == estate::login)
        {
            //添加对局信息
            int gid = glist.size();
            glist.insert({gid, gameinfo(n)});
            glist[gid].set_p2(oppo);
            glist[gid].set_rdy(n, true);
            ulist[n].gid = gid;
            ulist[oppo].gid = gid;
            pthread_rwlock_unlock(&rwlock);
        }
        else
        {
            // 请求失败
            pthread_rwlock_unlock(&rwlock);
            spkt.type = srvtype::gamerefuse;
            spkt.n[0] = name(oppo);
        }
        break;
    }
    //客户端接受邀请
    case 5:
    {
        std::string oppo = cpkt.n.to_string();
        std::string n = *(std::string *)pthread_getspecific(key);
        ERR(n == "", "client not login ")
        pthread_rwlock_wrlock(&rwlock);
        int gid = ulist[n].gid;
        if (glist.find(gid) != glist.end())
        {
            //接受邀请
            glist[gid].set_rdy(n, true);
            if(glist[gid].is_rdy(oppo))
            {
                glist[gid].round = 1;
            }
            pthread_rwlock_unlock(&rwlock);
            spkt.type = srvtype::gamestart;
            spkt.round = 1;
        }
        else
        {
            //接受失败
            pthread_rwlock_unlock(&rwlock);
            spkt.type = srvtype::gamerefuse;
            spkt.n[0] = name(n);
        }
        break;
    }
    //客户端拒绝邀请
    case 6:
    {
        std::string oppo = cpkt.n.to_string();
        std::string n = *(std::string *)pthread_getspecific(key);
        ERR(n == "", "client not login ")
        pthread_rwlock_wrlock(&rwlock);
        int gid = ulist[n].gid;
        if (glist.find(gid) != glist.end())
        {
            //拒绝邀请
            glist[gid].set_refuse(n, true);
        }
        pthread_rwlock_unlock(&rwlock);
        break;
    }
    //客户端退出游戏
    case 7:
    {
        std::string n = *(std::string *)pthread_getspecific(key);
        ERR(n == "", "client not login ")
        pthread_rwlock_wrlock(&rwlock);
        int gid = ulist[n].gid;
        if (glist.find(gid) != glist.end())
        {
            //退出游戏
            glist[gid].set_quit(n, true);
        }
        pthread_rwlock_unlock(&rwlock);
        break;
    }
    //客户端发送答案
    case 8:
    {
        std::string n = *(std::string *)pthread_getspecific(key);
        ERR(n == "", "client not login ")
        pthread_rwlock_wrlock(&rwlock);
        int gid = ulist[n].gid;
        if (glist.find(gid) != glist.end())
        {
            //发送答案
            glist[gid].set_ans(n, cpkt.ans);
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
}
