#include "client_game.h"

int stage = 0;
int res = 0;
std::map<int, fri> flist;
int round = 0;
int roundres = -1;
player self;
player oppo;
srvpkt spkt;
clipkt cpkt;

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
    if(stage == 0)
    {
        printf("Welcome to Rock-Paper-Scissors Game!\n");
        printf("(1) Login, (#) Exit\n");
    }
    //输入用户名
    else if(stage == 1)
    {
        if(res == 0) {printf("Please input your username:\n");}
        else if(res == 1) {printf("Username should be less than 16 characters, please try again.\n");}
        else if(res == 3) {printf("Username has been used, please try again.\n");}
    }
    else if(stage == 2)
    {
        if(res == 0)
        {
            CLS
            printf("Welcome, %s!\n", self.name.c_str());
            for (int i = 0; i < flist.size(); i++)
            {
                printf("%d. %-8s (%-8s)\n", i + 1, flist[i].name.c_str(), flist[i].st == estate::login ? "online" : "in game");
            }          
            printf("(2) Play with a friend, (#) Exit\n");
        }
        //你退出游戏
        else if(res == 2)
        {
            printf("You have left the game.\n");
        }
        //有邀请
        else if(res == 4)
        {
            printf("You have an invitation from %s, do you accept? (y/n)\n", oppo.name.c_str());
        }
        //邀请被拒绝
        else if(res == 5)
        {
            printf("Your invitation has been rejected.\n");
        }
        //对方退出游戏
        else if(res == 7)
        {
            printf("Your opponent has left the game.\n");
        }
    }
    //输入对战用户名
    else if(stage == 3)
    {
        if(res == 0) {printf("Please input the username of your opponent: ");}
        else if(res == 1) {printf("Username should be less than 16 characters, please try again.\n");}
    }
    //游戏中
    else if(stage == 4)
    {
        if(res == 0 || res == 10)
        {
            CLS
            printf("Round %d\n", round);
            printf("Your score: %d\n", self.score);
            printf("Opponent's score: %d\n", oppo.score);
            printf("(3) Left the game, (#) Exit\n");
            printf("Your turn, please input your answer (r/p/s): ");
        }
        else if(res == 1) {printf("Invalid input, please try again.\n");}
        //回合结束
        else if(res == 6)
        {
            CLS
            printf("Round %d ends.\n", round);
            printf("Your score: %d\n", self.score);
            printf("Opponent's score: %d\n", oppo.score);
            printf("Your answer: %s\n", self.ans == answer::rock ? "Rock" : self.ans == answer::paper ? "Paper" : self.ans == answer::scissors ? "Scissors" : "Unknown");
            printf("Opponent's answer: %s\n", oppo.ans == answer::rock ? "Rock" : oppo.ans == answer::paper ? "Paper" : oppo.ans == answer::scissors ? "Scissors" : "Unknown");
            printf("Round result: %s\n", roundres == 1 ? "You win" : roundres == 0 ? "You lose" : roundres == 2 ? "Draw" : "Unknown");
            printf("Press c to continue...\n");
        }
    }
    //等待游戏开始
    else if(stage == 5)
    {
        if(res == 0)
        {
            CLS
            printf("Waiting for the game to start...\n");
            printf("(3) Left the game, (#) Exit\n");
        }
    }
    //显示游戏结果
    else if(stage == 6)
    {
        if(res == 0)
        {
            CLS
            printf("Game ends.\n");
            printf("Your score: %d\n", self.score);
            printf("Opponent's score: %d\n", oppo.score);
            printf("Game result: %s\n", self.score > oppo.score ? "You win" : self.score < oppo.score ? "You lose" : "Draw");
            printf("Press c to continue...\n");
        }
    }
    res = 0;
}

void client_end_round()
{
    if (self.ans == oppo.ans) { roundres = 2;}
    else if (self.ans == answer::rock && oppo.ans == answer::scissors) {self.score++;roundres = 1;}
    else if (self.ans == answer::paper && oppo.ans == answer::rock) {self.score++;roundres = 1;}
    else if (self.ans == answer::scissors && oppo.ans == answer::paper) {self.score++;roundres = 1;}
    else {oppo.score++;roundres = 0;}
}
