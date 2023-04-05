#include "server_nb.h"

//全局变量

std::map<std::string, state> ulist;
std::map<int, gameinfo> glist;
bool uchanged = false;
int readcnt = 0;
int threadcnt = 1;
pthread_rwlock_t rwlock;
//仅该文件内可见

std::map<std::string, state>::iterator uit;
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
    CLS
    printf("**********online user list********************game info***********\n");
    int i = 0, g = 0;
    uit = ulist.begin();
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
        if (g < glist.size())
        {
            printf("*%02d. player1:%-8s player2:%-8s*\n", g + 1, glist[g].p1.name.c_str(), glist[g].p2.name.c_str());
            g++;
        }
        else
        {
            printf("*                             *\n");//31个空格
        }
    }
    printf("******************************************************************\n");
}

void *server_thread(void *arg)
{
    int cfd = *(int*)arg;

    //打印客户端ip和端口
    // struct sockaddr_in caddr;
    // socklen_t addrlen = sizeof(caddr);
    // getpeername(cfd, (struct sockaddr*)&caddr, &addrlen);
    // printf("client ip=%s port=%d\n", inet_ntoa(caddr.sin_addr), ntohs(caddr.sin_port));

    fd_set rset, allset;
    timeval rtv, tv;
    FD_ZERO(&allset);
    FD_SET(cfd, &allset);
    tv.tv_sec = 0;
    tv.tv_usec = 500000;

    while(true)
    {
        rset = allset;
        rtv = tv;
        int rc = select(cfd + 1, &rset, NULL, NULL, &rtv);
        ERR(rc < 0, "select failed ")
        if(FD_ISSET(cfd, &rset))
        {
            //接收客户端请求
            clipkt cpkt;
            int rt1 = recv(cfd, &cpkt, sizeof(cpkt), 0);
            srvpkt spkt;
            server_handle(rt1, cpkt, spkt);
            // 发送响应
            int rt2 = send(cfd, &spkt, sizeof(spkt), 0);
            ERR(rt2 < 0, "send failed ")
        }
    }
    close(cfd);
    return NULL;
}

void server_handle(int res, clipkt &p, srvpkt &spkt)
{
    if(res == 0)
    {
        std::string n = *(std::string *)pthread_getspecific(key);
        ERR(n == "", "client not login ")
        //客户端断开连接
        pthread_rwlock_wrlock(&rwlock);
        //从游戏列表和用户列表中删除
        glist.erase(ulist[n].gid);
        ulist.erase(n);
        uchanged = true;
        pthread_rwlock_unlock(&rwlock);
    }
    else if(res > 0)
    {
        switch(p.type)
        {
            case clitype::login:
            {
                std::string n = p.n.to_string();
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
            case clitype::gamerequest:
            {
                std::string oppo = p.n.to_string();
                std::string n = *(std::string *)pthread_getspecific(key);
                ERR(n == "", "client not login ")
                pthread_rwlock_rdlock(&rwlock);
                if (ulist.find(oppo) != ulist.end() && ulist[oppo].st == estate::login)
                {
                    pthread_rwlock_unlock(&rwlock);
                    spkt.type = srvtype::gamerequest;
                    spkt.n[0] = name(n);
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
        }
    }
}
