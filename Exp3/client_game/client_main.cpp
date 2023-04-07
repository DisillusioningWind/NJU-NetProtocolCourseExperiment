#include "client_game.h"

int main(int argc, char** argv)
{
    const char *ip = argc > 1 ? argv[1] : SERV_IP;
    int cfd = client_connect(ip, SERV_PORT);
    fd_set rset, allset;
    char buf[100];
    FD_ZERO(&allset);
    FD_SET(0, &allset);
    FD_SET(cfd, &allset);

    while(true)
    {
        client_show_menu();
        rset = allset;
        int nready = select(cfd + 1, &rset, NULL, NULL, NULL);
        if(FD_ISSET(cfd, &rset))
        {
            int n = read(cfd, &cpkt, sizeof(clipkt));
            if(n == 0)
            {
                printf("server closed\n");
                break;
            }
        }
        if(FD_ISSET(0, &rset))
        {
            fgets(buf, 100, stdin);
            buf[strcspn(buf, "\n")] = 0;
            if(strncmp(buf, "#", 1) == 0) { break; }
            //登录
            else if(stage == 0 && strncmp(buf, "1", 1) == 0) {stage = 1;}
            else if(stage == 1 && strlen(buf) > 0)
            {
                int len = strlen(buf);
                if(len > 15)
                {
                    stage = 0;
                    res = 1;
                }
                else
                {
                    cpkt.type = clitype::login;
                    cpkt.n = buf;
                    send(cfd, &cpkt, sizeof(clipkt), 0);
                }
            }
            //邀请/接受邀请
            else if(stage == 2 && strncmp(buf,"2", 1) == 0) {stage = 3;}
            else if(stage == 2 && strncmp(buf, "y", 1) == 0) {stage = 4;}
            else if(stage == 2 && strncmp(buf, "n", 1) == 0) {stage = 2;}
            else if(stage == 3 && strlen(buf) > 0)
            {
                int len = strlen(buf);
                if(len > 15)
                {
                    stage = 2;
                    res = 1;
                }
                else
                {
                    cpkt.type = clitype::gamerequest;
                    cpkt.n = buf;
                    send(cfd, &cpkt, sizeof(clipkt), 0);
                }
            }
            //出拳
            else if(stage == 4 && strlen(buf) > 0)
            {
                if(strncmp(buf,"rock",4) == 0) {cpkt.ans == answer::rock;}
                else if(strncmp(buf,"paper",5) == 0) {cpkt.ans == answer::paper;}
                else if(strncmp(buf,"scissors",8) == 0) {cpkt.ans == answer::scissors;}
                else {res = 1;}
                if(res == 0)
                {
                    cpkt.type = clitype::gameanswer;
                    cpkt.round = round;
                    cpkt.n = self.name;
                    send(cfd, &cpkt, sizeof(clipkt), 0);
                }
            }
        }
    }
    close(cfd);
    return 0;
}