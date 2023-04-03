#include "server_nb.h"

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
    if (rt1 < 0) {
        error(1, errno, "bind failed ");
    }
    //监听
    int rt2 = listen(listenfd, LISTENQ);
    if (rt2 < 0)
    {
        error(1, errno, "listen failed ");
    }
    //忽略SIGPIPE信号
    signal(SIGPIPE, SIG_IGN);

    return listenfd;
}

void server_set_nonblocking(int fd) {
    fcntl(fd, F_SETFL, O_NONBLOCK);
}

void *server_thread(void *arg)
{
    int cfd = *(int*)arg;
    //打印客户端ip和端口
    struct sockaddr_in caddr;
    socklen_t addrlen = sizeof(caddr);
    getpeername(cfd, (struct sockaddr*)&caddr, &addrlen);
    printf("client ip=%s port=%d\n", inet_ntoa(caddr.sin_addr), ntohs(caddr.sin_port));

    close(cfd);
    return NULL;
}
