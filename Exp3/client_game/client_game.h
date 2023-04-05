#pragma once
#include "../common.h"

#define SERV_IP "127.0.0.1"

//创建客户端套接字
int client_connect(const char* ip, int port);
//客户端菜单
void client_show_menu();
