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
        cpkt.zero();
        spkt.zero();
        int nready = select(cfd + 1, &rset, NULL, NULL, NULL);
        if(FD_ISSET(cfd, &rset))
        {
            int rt = recv(cfd, &spkt, sizeof(srvpkt), 0);
            ERR(rt < 0, "recv failed")
            else if(rt == 0) {break;}
            else
            {
                if(stage == 1 && spkt.type == srvtype::loginok) {stage = 2;}
                else if(stage == 1 && spkt.type == srvtype::loginfail)
                {
                    stage = 1;
                    res = 3;
                    self.name.clear();
                }
                else if(stage == 2 && spkt.type == srvtype::gamerequest)
                {
                    oppo.name = spkt.u[0].n.to_string();
                    res = 4;
                }
                else if(stage == 3 && spkt.type == srvtype::gamerefuse)
                {
                    stage = 2;
                    res = 5;
                    oppo.zero();
                }
                else if((stage == 3 || stage == 5) && spkt.type == srvtype::gamestart)
                {
                    stage = 4;
                }
                else if(stage == 4 && spkt.type == srvtype::gameanswer)
                {
                    round = spkt.round;
                    oppo.ans = spkt.ans;
                    client_end_round();
                    if(self.score >= 2)
                    {
                        stage = 6;
                        res = 8;
                    }
                    else if(oppo.score >= 2)
                    {
                        stage = 6;
                        res = 9;
                    }
                    else
                    {
                        res = 6;
                    }
                }
                else if(stage == 4 && spkt.type == srvtype::gamequit)
                {
                    oppo.zero();
                    stage = 2;
                    res = 7;
                }
                else if(spkt.type == srvtype::userinfo)
                {
                    flist.clear();
                    for(int i = 0; i < spkt.res_num; i++)
                    {
                        fri f;
                        f.name = spkt.u[i].n.to_string();
                        f.st = spkt.u[i].st;
                        flist[i] = f;
                    }
                }
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
                    stage = 1;
                    res = 1;
                }
                else
                {
                    self.name = buf;
                    cpkt.type = clitype::login;
                    cpkt.n = buf;
                    send(cfd, &cpkt, sizeof(clipkt), 0);
                }
            }
            //邀请/接受邀请
            else if(stage == 2 && strncmp(buf,"2", 1) == 0) {stage = 3;}
            else if(stage == 2 && strncmp(buf, "y", 1) == 0)
            {
                cpkt.type = clitype::gameaccept;
                cpkt.n = oppo.name;
                send(cfd, &cpkt, sizeof(clipkt), 0);
                stage = 5;
            }
            else if(stage == 2 && strncmp(buf, "n", 1) == 0)
            {
                cpkt.type = clitype::gamerefuse;
                cpkt.n = oppo.name;
                send(cfd, &cpkt, sizeof(clipkt), 0);
                stage = 2;
            }
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
                    oppo.name = buf;
                    cpkt.type = clitype::gamerequest;
                    cpkt.n = buf;
                    send(cfd, &cpkt, sizeof(clipkt), 0);
                }
            }
            //出拳
            else if(stage == 4 && strlen(buf) > 0)
            {
                if(strncmp(buf,"rock",4) == 0) {self.ans == answer::rock;}
                else if(strncmp(buf,"paper",5) == 0) {self.ans == answer::paper;}
                else if(strncmp(buf,"scissors",8) == 0) {self.ans == answer::scissors;}
                else if(strncmp(buf,"3", 1) == 0)
                {
                    cpkt.type = clitype::gamequit;
                    cpkt.n = self.name;
                    send(cfd, &cpkt, sizeof(clipkt), 0);
                    stage = 2;
                    res = 2;
                }
                else {res = 1;}
                if(res == 0)
                {
                    cpkt.type = clitype::gameanswer;
                    cpkt.round = round;
                    cpkt.n = self.name;
                    send(cfd, &cpkt, sizeof(clipkt), 0);    
                }
            }
            else if(stage == 6){stage = 2;}
        }
    }
    close(cfd);
    return 0;
}