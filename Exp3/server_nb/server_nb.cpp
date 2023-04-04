#include "server_nb.h"

//全局变量

std::map<std::string, state> ulist;
std::map<int, gameinfo> glist;
bool uchanged = false;

std::map<std::string, state>::iterator uit;

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
    struct sockaddr_in caddr;
    socklen_t addrlen = sizeof(caddr);
    getpeername(cfd, (struct sockaddr*)&caddr, &addrlen);
    printf("client ip=%s port=%d\n", inet_ntoa(caddr.sin_addr), ntohs(caddr.sin_port));

    while(true)
    {
        //接收客户端消息
        char buf[30];
        int n = recv(cfd, buf, sizeof(buf), 0);
        if (n <= 0)
        {
            if (n == 0)
            {
                printf("client close\n");
            }
            else
            {
                printf("recv error\n");
            }
            break;
        }
        buf[n] = '\0';
        //处理客户端消息
        uit = ulist.find(buf);
        if (uit != ulist.end())
        {
            //用户已经登录
            send(cfd, "login failed", 12, 0);
        }
        else
        {
            //用户未登录
            send(cfd, "login succes", 12, 0);
            //添加用户
            ulist.insert(std::pair<std::string, state>(buf, state{estate::login}));
            uchanged = true;
        }
    }
    close(cfd);
    return NULL;
}
