#include "wea_cli.h"

//delete '\n'
char* wgets(char data[],int cnt)
{
    char *res = fgets(data, cnt, stdin);
    char *find = strchr(data, '\n');
    if(find)
        *find = 0;
    return res;
}

void showMainMenu()
{
    printf("Welcome to NJUCS Weather Forecast Program!\n");
    printf("Please input City Name in Chinese pinyin(e.g. nanjing or beijing)\n");
    printf("(c)cls,(#)exit\n");
}

void showSubMenu()
{
    printf("Please enter the given number to query\n");
    printf("1.today\n");
    printf("2.three days from today\n");
    printf("3.custom day by yourself\n");
    printf("(r)back,(c)cls,(#)exit\n");
}

void showMenu(int lv, bool need)
{
    if(!need) return;
    if(lv == 1) showMainMenu();
    else if(lv == 2) showSubMenu();
    else if(lv == 3);
    else printf("showMenu failed\n");
}

int main(int argc, char** argv)
{
    int sockfd;
    struct sockaddr_in servaddr;
    char query[MAXLINE];
    char cityname[30] = {0};
    int menulevel = 1;
    bool menushow = true;

    sockfd = socket(AF_INET, SOCK_STREAM, 0);

    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = inet_addr(SERV_ADDR);
    servaddr.sin_port = htons(SERV_PORT);

    int consuc = connect(sockfd, (struct sockaddr *)&servaddr, sizeof(servaddr));
    if(consuc != 0)
    {
        perror("");
        printf("Can Not Connect To Server %s\n", SERV_ADDR);
        exit(1);
    }

    fd_set recvmask;
    fd_set recvall;

    FD_ZERO(&recvall);
    FD_SET(0, &recvall);
    FD_SET(sockfd, &recvall);

    while (1)
    {
        showMenu(menulevel, menushow);
        recvmask = recvall;
        int rc = select(sockfd + 1, &recvmask, NULL, NULL, NULL);
        if(rc <= 0)
        {
            perror("select failed");
            exit(2);
        }
        if(FD_ISSET(sockfd,&recvmask))
        {
            recvpkt rpkt;
            int rvsuc = recv(sockfd, &rpkt, sizeof(rpkt), 0);
            if(rvsuc <= 0)
            {
                perror("recv failed");
                exit(3);
            }
            if(menulevel == 1 && rpkt.isCitypkt())
            {
                menushow = true;
                menulevel = 2;
                int ssuc = system("clear");
                if(ssuc == -1)
                {
                    printf("cls failed\n");
                }
            }
            else if(menulevel == 1 && rpkt.isWorngpkt())
            {
                menushow = false;
                printf("Sorry, Server does not have weather information for city %s,please try again\n", rpkt.cname);
            }
            else if(menulevel == 2 && rpkt.isWeapkt())
            {
                menushow = false;
                printf("City :%s    Today is :%d/%02d/%d    Weather information is as follows:\n", rpkt.cname, rpkt.getYear(), rpkt.getMon(), rpkt.getDay());
                if(rpkt.getType() == 'A')
                {
                    if(rpkt.num == 0x01)
                    {
                        printf("Today's Weather is: %s; Temp:%d\n", rpkt.con[0].getWeather(), rpkt.con[0].getTemp());
                    }
                    else
                    {
                        printf("The %dth day's Weather is: %s;  Temp:%d\n", rpkt.num, rpkt.con[0].getWeather(), rpkt.con[0].getTemp());
                    }
                }
                else if(rpkt.getType() == 'B')
                {
                    for (int i = 0; i < rpkt.num; i++)
                    {
                        printf("The %dth day's Weather is: %s;  Temp:%d\n", i + 1, rpkt.con[i].getWeather(), rpkt.con[i].getTemp());
                    }             
                }
            }
            else if(menulevel == 2 && rpkt.isNoinfopkt())
            {
                menushow = false;
                printf("Sorry, no given day's weather for city %s\n", rpkt.cname);
            }
        }
        if(FD_ISSET(0,&recvmask) && wgets(query,MAXLINE) != NULL)
        {
            if(strncmp(query,"#",1) == 0)
            {
                FD_CLR(0, &recvall);
                int clsuc = close(sockfd);
                if(clsuc != 0)
                {
                    perror("close failed");
                    exit(4);
                }
                printf("closed\n");
                exit(0);
            }
            else if(strncmp(query,"c",1) == 0)
            {
                menushow = true;
                int ssuc = system("clear");
                if(ssuc == -1)
                {
                    printf("cls failed\n");
                }
            }
            else if(menulevel == 1)
            {
                menushow = false;
                if(strlen(query) >= 30)
                {
                    printf("your input must be less than 30 words, please try again:\n");
                    continue;
                }
                strcpy(cityname, query);
                sendpkt pcity = sendpkt(0x01, 0x00, query, 0x00);
                int sdsuc = send(sockfd, &pcity, sizeof(pcity), 0);
                if (sdsuc == -1)
                {
                    perror("send city failed");
                    exit(5);
                }
            }
            else if(menulevel == 2 && strncmp(query,"r",1) == 0)
            {
                menushow = true;
                menulevel = 1;
                int ssuc = system("clear");
                if(ssuc == -1)
                {
                    printf("cls failed\n");
                }
            }
            else if(menulevel == 2 && strncmp(query,"1",1) == 0)
            {
                menushow = false;
                sendpkt ptoday = sendpkt(0x02, 0x01, cityname, 0x01);
                int sdsuc = send(sockfd, &ptoday, sizeof(ptoday), 0);
                if (sdsuc == -1)
                {
                    perror("send query failed");
                    exit(6);
                }
            }
            else if(menulevel == 2 && strncmp(query,"2",1) == 0)
            {
                menushow = false;
                sendpkt ptoday = sendpkt(0x02, 0x02, cityname, 0x03);
                int sdsuc = send(sockfd, &ptoday, sizeof(ptoday), 0);
                if (sdsuc == -1)
                {
                    perror("send query failed");
                    exit(6);
                }
            }
            else if(menulevel == 2 && strncmp(query,"3",1) == 0)
            {
                menushow = false;
                menulevel = 3;
                printf("Please enter the day number(below 10,e.g. 1 means today):\n");
            }
            else if(menulevel == 3)
            {
                menushow = false;
                int qday = atoi(query);
                if(qday < 1 || qday > 9)
                {
                    printf("input error, please try again:\n");
                    continue;
                }
                menulevel = 2;
                sendpkt ptoday = sendpkt(0x02, 0x01, cityname, (uint8_t)qday);
                int sdsuc = send(sockfd, &ptoday, sizeof(ptoday), 0);
                if (sdsuc == -1)
                {
                    perror("send query failed");
                    exit(6);
                }
            }
            else
            {
                menushow = false;
                printf("input error, please try again:\n");
            }
        }
    }
}