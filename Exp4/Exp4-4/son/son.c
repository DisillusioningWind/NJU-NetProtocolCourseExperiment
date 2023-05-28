// 文件名: son/son.c
// 描述: 这个文件实现SON进程
// SON进程首先连接到所有邻居, 然后启动listen_to_neighbor线程, 每个该线程持续接收来自一个邻居的进入报文, 并将该报文转发给SIP进程.
// 然后SON进程等待来自SIP进程的连接. 在与SIP进程建立连接之后, SON进程持续接收来自SIP进程的sendpkt_arg_t结构, 并将接收到的报文发送到重叠网络中.

#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <signal.h>
#include <sys/utsname.h>
#include <assert.h>

#include "../common/constants.h"
#include "../common/pkt.h"
#include "son.h"
#include "../topology/topology.h"
#include "neighbortable.h"

// 你应该在这个时间段内启动所有重叠网络节点上的SON进程
#define SON_START_DELAY 25

// 将邻居表声明为一个全局变量
nbr_entry_t *nt;
// 邻居表中的邻居数
int nbrSumNum = 0;
// 将与SIP进程之间的TCP连接声明为一个全局变量
int sip_conn;

// 这个线程打开TCP端口CONNECTION_PORT, 等待节点ID比自己大的所有邻居的进入连接,
// 在所有进入连接都建立后, 这个线程终止.
void *waitNbrs(void *arg)
{
	int sockfd, connfd, res, nbrNum = 0, myNodeID;
	struct sockaddr_in my_addr;
	struct sockaddr_in their_addr;
	unsigned int sin_size = sizeof(struct sockaddr_in);
	// 创建TCP套接字
	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd < 0)
	{
		perror("socket");
		exit(1);
	}
	// 设置端口复用
	int opt = 1;
	res = setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
	if (res == -1)
	{
		perror("setsockopt");
		exit(1);
	}
	// 设置本地地址结构
	my_addr.sin_family = AF_INET;
	my_addr.sin_port = htons(CONNECTION_PORT);
	my_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	// 绑定TCP套接字到本地地址结构
	res = bind(sockfd, (struct sockaddr *)&my_addr, sizeof(struct sockaddr));
	if (res == -1)
	{
		perror("bind");
		exit(1);
	}
	// 监听TCP套接字
	res = listen(sockfd, MAX_NODE_NUM);
	if (res == -1)
	{
		perror("listen");
		exit(1);
	}
	// 获得ID比自己大的邻居的数量
	myNodeID = topology_getMyNodeID();
	for (int i = 0; i < nbrSumNum; i++)
	{
		if (nt[i].nodeID > myNodeID)
		{
			nbrNum++;
		}
	}
	// 没有ID比自己大的邻居
	if (nbrNum == 0)
		return NULL;
	// 接受来自邻居的连接
	int conNum = 0;
	while (1)
	{
		connfd = accept(sockfd, (struct sockaddr *)&their_addr, &sin_size);
		if (connfd == -1)
		{
			perror("accept");
			continue;
		}
		int nodeID = topology_getNodeIDfromip(&their_addr.sin_addr);
		printf("accept conn from ip:%s\n", inet_ntoa(their_addr.sin_addr));
		if (nodeID > myNodeID)
		{
			nt_addconn(nt, nodeID, connfd);
			conNum++;
		}
		else
		{
			close(connfd);
		}
		// 所有ID更大的邻居都已连接
		if (conNum >= nbrNum)
		{
			break;
		}
	}
	return NULL;
}

// 这个函数连接到节点ID比自己小的所有邻居.
// 在所有外出连接都建立后, 返回1, 否则返回-1.
int connectNbrs()
{
	int nbrNum = 0;
	int myNodeID = topology_getMyNodeID();
	// 获得ID比自己小的邻居的数量
	for (int i = 0; i < nbrSumNum; i++)
	{
		if (nt[i].nodeID < myNodeID)
			nbrNum++;
	}
	// 没有ID比自己小的邻居
	if (nbrNum == 0)
		return 1;
	// 连接到ID比自己小的邻居
	for (int i = 0; i < nbrSumNum; i++)
	{
		if (nt[i].nodeID < myNodeID && nt[i].conn == -1)
		{
			int sockfd, res;
			// 创建TCP套接字
			sockfd = socket(AF_INET, SOCK_STREAM, 0);
			if (sockfd < 0)
			{
				perror("socket");
				exit(1);
			}
			// 设置端口复用
			int opt = 1;
			res = setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
			// 设置对方地址结构
			struct sockaddr_in their_addr;
			their_addr.sin_family = AF_INET;
			their_addr.sin_port = htons(CONNECTION_PORT);
			their_addr.sin_addr.s_addr = nt[i].nodeIP;
			// 连接到对方
			res = connect(sockfd, (struct sockaddr *)&their_addr, sizeof(struct sockaddr));
			if (res == -1)
			{
				printf("connect to node %d failed, ip:%s\n", nt[i].nodeID, inet_ntoa(their_addr.sin_addr));
				perror("connect");
				exit(0);
			}
			nt_addconn(nt, nt[i].nodeID, sockfd);
			printf("connect to node %d\n", nt[i].nodeID);
		}
	}
	return 1;
}

// 每个listen_to_neighbor线程持续接收来自一个邻居的报文. 它将接收到的报文转发给SIP进程.
// 所有的listen_to_neighbor线程都是在到邻居的TCP连接全部建立之后启动的.
void *listen_to_neighbor(void *arg)
{
	int idx = *(int *)arg;
	int connfd = nt[idx].conn;
	if (connfd == -1)
	{
		printf("connfd == -1\n");
		return NULL;
	}
	while (1)
	{
		sip_pkt_t pkt;
		int res = recvpkt(&pkt, connfd);
		if (res == -1)
		{
			close(connfd);
			nt[idx].conn = -1;
			printf("close conn to node %d\n", nt[idx].nodeID);
			return NULL;
		}
		if (sip_conn != -1)
		{
			// 将报文转发给SIP进程
			res = forwardpktToSIP(&pkt, sip_conn);
			if (res == -1)
			{
				perror("forwardpktToSIP");
				printf("strange sip closed\n");
				continue;
			}
		}
	}
	return NULL;
}

// 这个函数打开TCP端口SON_PORT, 等待来自本地SIP进程的进入连接.
// 在本地SIP进程连接之后, 这个函数持续接收来自SIP进程的sendpkt_arg_t结构, 并将报文发送到重叠网络中的下一跳.
// 如果下一跳的节点ID为BROADCAST_NODEID, 报文应发送到所有邻居节点.
void waitSIP()
{
	int sockfd, res;
	struct sockaddr_in my_addr;
	// 创建TCP套接字
	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd < 0)
	{
		perror("socket");
		exit(1);
	}
	// 设置端口复用
	int opt = 1;
	res = setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
	// 设置本地地址结构
	my_addr.sin_family = AF_INET;
	my_addr.sin_port = htons(SON_PORT);
	my_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	// 绑定TCP套接字到本地地址结构
	res = bind(sockfd, (struct sockaddr *)&my_addr, sizeof(struct sockaddr));
	if (res == -1)
	{
		perror("bind");
		exit(1);
	}
	// 监听TCP套接字
	res = listen(sockfd, MAX_NODE_NUM);
	if (res == -1)
	{
		perror("listen");
		exit(1);
	}
reaccept:
	// 接受来自SIP进程的连接
	sip_conn = accept(sockfd, NULL, NULL);
	if (sip_conn == -1)
	{
		perror("accept");
		exit(1);
	}
	// 持续接收来自SIP进程的报文
	printf("accept sip conn\n");
	while (1)
	{
		sip_pkt_t arg;
		int nextNodeID;
		res = getpktToSend(&arg, &nextNodeID, sip_conn);
		if (res == -1)
		{
			// 如果SIP进程关闭了连接, 重新等待SIP进程的连接
			perror("getpktToSend");
			printf("SIP closed, wait for reconnecting\n");
			close(sip_conn);
			sip_conn = -1;
			goto reaccept;
		}
		// 如果下一跳的节点ID为BROADCAST_NODEID, 报文应发送到所有邻居节点
		if (nextNodeID == BROADCAST_NODEID)
		{
			for (int i = 0; i < nbrSumNum; i++)
			{
				if (nt[i].conn == -1)
					continue;
				res = sendpkt(&arg, nt[i].conn);
				if (res == -1)
				{
					perror("sendpkt");
					continue;
				}
			}
		}
		else
		{
			// 将报文发送到重叠网络中的下一跳
			for (int i = 0; i < nbrSumNum; i++)
			{
				if (nt[i].nodeID == nextNodeID)
				{
					// 如果下一跳的TCP连接已经关闭，直接不管
					if (nt[i].conn == -1)
						break;
					res = sendpkt(&arg, nt[i].conn);
					if (res == -1)
					{
						perror("sendpkt");
						continue;
					}
					break;
				}
			}
		}
	}
	printf("strange sip closed in waitSIP\n");
	return;
}

// 这个函数停止重叠网络, 当接收到信号SIGINT时, 该函数被调用.
// 它关闭所有的连接, 释放所有动态分配的内存.
void son_stop()
{
	//发送该节点不可达的sip报文
	sip_pkt_t pkt;
	pkt.header.type = CLOSE;
	pkt.header.src_nodeID = myID;
	pkt.header.dest_nodeID = BROADCAST_NODEID;
	pkt.header.length = 0;
	for (int i = 0; i < nbrSumNum; i++)
	{
		if (nt[i].conn == -1)
			continue;
		int res = sendpkt(&pkt, nt[i].conn);
	}
	printf("son send close packet\n");
	// 释放所有动态分配的内存
	nt_destroy(nt);
	// 关闭与SIP进程的连接
	if (sip_conn != -1)
		close(sip_conn);
	exit(0);
}

int main()
{
	// 启动重叠网络初始化工作
	printf("Overlay network: Node %d initializing...\n", topology_getMyNodeID());

	// 创建一个邻居表
	topology_init();
	nt = nt_create();
	if (nt == NULL)
	{
		printf("Open topology file failed\n");
		exit(1);
	}
	nbrSumNum = topology_getNbrNum();
	// 将sip_conn初始化为-1, 即还未与SIP进程连接
	sip_conn = -1;

	// 注册信号句柄, 用于终止进程
	signal(SIGINT, son_stop);
	signal(SIGTERM, son_stop);

	// 打印所有邻居
	for (int i = 0; i < nbrSumNum; i++)
	{
		printf("Overlay network: neighbor %d:%d\n", i + 1, nt[i].nodeID);
	}
	// 启动waitNbrs线程, 等待节点ID比自己大的所有邻居的进入连接
	pthread_t waitNbrs_thread;
	pthread_create(&waitNbrs_thread, NULL, waitNbrs, (void *)0);

	// 等待其他节点启动
	sleep(SON_START_DELAY);

	// 连接到节点ID比自己小的所有邻居
	connectNbrs();

	// 等待waitNbrs线程返回
	pthread_join(waitNbrs_thread, NULL);

	// 此时, 所有与邻居之间的连接都建立好了
	printf("Overlay network: all neighbors connected...\n");

	// 创建线程监听所有邻居
	for (int i = 0; i < nbrSumNum; i++)
	{
		int *idx = (int *)malloc(sizeof(int));
		*idx = i;
		pthread_t nbr_listen_thread;
		pthread_create(&nbr_listen_thread, NULL, listen_to_neighbor, (void *)idx);
	}
	printf("Overlay network: node initialized...\n");
	printf("Overlay network: waiting for connection from SIP process...\n");

	// 等待来自SIP进程的连接
	waitSIP();
}
