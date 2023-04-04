#include "client_game.h"

int client_connect(const char *ip, int port)
{
    //创建套接字
    int cfd = socket(AF_INET, SOCK_STREAM, 0);
    //填充套接字地址
    struct sockaddr_in server_addr;
    bzero(&server_addr, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    inet_pton(AF_INET, ip, &server_addr.sin_addr);
    //连接
    int rt1 = connect(cfd, (struct sockaddr *) &server_addr, sizeof(server_addr));
    ERR(rt1 < 0, "connect failed ")
    return cfd;
}

void client_show_menu()
{
    
}
