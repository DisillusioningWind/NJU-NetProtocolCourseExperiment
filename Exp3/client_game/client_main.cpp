#include "client_game.h"

int main(int argc, char** argv)
{
    const char *ip = argc > 1 ? argv[1] : SERV_IP;
    int cfd = client_connect(ip, SERV_PORT);
    char buf[100];
    while(true)
    {
        //client_show_menu();
        fgets(buf, 100, stdin);
        if(strncmp(buf, "#", 1) == 0) { break; }
        write(cfd, buf, strlen(buf) - 1);
        int n = read(cfd, buf, 100);
        buf[n] = 0;
        printf("%s\n", buf);
    }
    close(cfd);
    return 0;
}