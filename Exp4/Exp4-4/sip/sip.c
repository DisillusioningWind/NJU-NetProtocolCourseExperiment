//文件名: sip/sip.c
//
//描述: 这个文件实现SIP进程  

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <strings.h>
#include <arpa/inet.h>
#include <signal.h>
#include <netdb.h>
#include <assert.h>
#include <sys/utsname.h>
#include <pthread.h>
#include <unistd.h>

#include "../common/constants.h"
#include "../common/pkt.h"
#include "../common/seg.h"
#include "../topology/topology.h"
#include "sip.h"
#include "nbrcosttable.h"
#include "dvtable.h"
#include "routingtable.h"

//SIP层等待这段时间让SIP路由协议建立路由路径. 
#define SIP_WAITTIME 60

/**************************************************************/
//声明全局变量
/**************************************************************/
int son_conn; 			//到重叠网络的连接
int stcp_conn;			//到STCP的连接
nbr_cost_entry_t* nct;			//邻居代价表
dv_t* dv;				//距离矢量表
pthread_mutex_t* dv_mutex;		//距离矢量表互斥量
routingtable_t* routingtable;		//路由表
pthread_mutex_t* routingtable_mutex;	//路由表互斥量

/**************************************************************/
//实现SIP的函数
/**************************************************************/

//SIP进程使用这个函数连接到本地SON进程的端口SON_PORT.
//成功时返回连接描述符, 否则返回-1.
int connectToSON() {
	int sockfd, res;
	struct sockaddr_in servaddr;
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
	// 设置服务器地址结构
	servaddr.sin_family = AF_INET;
	servaddr.sin_port = htons(SON_PORT);
	servaddr.sin_addr.s_addr = inet_addr("127.0.0.1");
	// 连接到服务器
	res = connect(sockfd, (struct sockaddr *)&servaddr, sizeof(struct sockaddr));
	if (res == -1)
	{
		perror("connect");
		exit(1);
	}
	return sockfd;
}

//这个线程每隔ROUTEUPDATE_INTERVAL时间发送路由更新报文.路由更新报文包含这个节点
//的距离矢量.广播是通过设置SIP报文头中的dest_nodeID为BROADCAST_NODEID,并通过son_sendpkt()发送报文来完成的.
void* routeupdate_daemon(void* arg) {
	int res;
	sip_pkt_t pkt;
	pkt.header.src_nodeID = myID;
	pkt.header.dest_nodeID = BROADCAST_NODEID;
	pkt.header.type = ROUTE_UPDATE;
	pthread_mutex_lock(dv_mutex);
	memcpy(pkt.data, dv[nbrNum].dvEntry, sizeof(dv_entry_t) * sumNum);
	pthread_mutex_unlock(dv_mutex);
	pkt.header.length = sizeof(dv_entry_t) * sumNum;
	while (1)
	{
		sleep(ROUTEUPDATE_INTERVAL);
		if (son_conn > 0)
		{
			res = son_sendpkt(BROADCAST_NODEID, &pkt, son_conn);
			if (res == -1)
			{
				perror("son_sendpkt");
				close(son_conn);
				son_conn = -1;
				return NULL;
			}
		}
		else
			return NULL;
	}
	return NULL;
}

//这个线程处理来自SON进程的进入报文. 它通过调用son_recvpkt()接收来自SON进程的报文.
//如果报文是SIP报文,并且目的节点就是本节点,就转发报文给STCP进程. 如果目的节点不是本节点,
//就根据路由表转发报文给下一跳.如果报文是路由更新报文,就更新距离矢量表和路由表.
void* pkthandler(void* arg) {
	int res;
	sip_pkt_t pkt;
	seg_t seg;
	while (1)
	{
		res = son_recvpkt(&pkt, son_conn);
		if (res == -1)
		{
			perror("son_recvpkt");
			close(son_conn);
			son_conn = -1;
			return NULL;
		}
		if (pkt.header.type == SIP)
		{
			if (pkt.header.dest_nodeID == myID)
			{
				memcpy(&seg, pkt.data, sizeof(seg_t));
				res = forwardsegToSTCP(stcp_conn, pkt.header.src_nodeID, &seg);
				if (res == -1)
				{
					perror("stcp_conn_sendpkt");
					close(stcp_conn);
					stcp_conn = -1;
					return NULL;
				}
			}
			else
			{
				pthread_mutex_lock(routingtable_mutex);
				int next = routingtable_getnextnode(routingtable, pkt.header.dest_nodeID);
				pthread_mutex_unlock(routingtable_mutex);
				if (next == -1)
				{
					printf("no route to node %d\n", pkt.header.dest_nodeID);
					continue;
				}
				res = son_sendpkt(next, &pkt, son_conn);
				if (res == -1)
				{
					perror("son_sendpkt");
					close(son_conn);
					son_conn = -1;
					return NULL;
				}
			}
		}
		else if (pkt.header.type == ROUTE_UPDATE)
		{
			int i;
			int srcID = pkt.header.src_nodeID;
			pthread_mutex_lock(dv_mutex);
			//更新来源节点的距离矢量
			for(i = 0; i < nbrNum + 1; i++)
			{
				if(srcID == dv[i].nodeID)
				{
					memcpy(dv[i].dvEntry, pkt.data, sizeof(dv_entry_t) * sumNum);
					break;
				}
			}
			//更新自己的距离矢量
			for(i = 0; i < sumNum; i++)
			{
				int destID = dv[nbrNum].dvEntry[i].nodeID;
				int minCost = INFINITE_COST;
				int minID = -1;
				for(int j = 0; j < nbrNum; j++)
				{
					//D(destID) = min{c(selfID, interID) + D(interID, destID)}
					int interID = nct[j].nodeID;
					int tmpCost = nbrcosttable_getcost(nct, interID) + dvtable_getcost(dv, interID, destID);
					if(tmpCost < minCost)
					{
						minCost = tmpCost;
						minID = interID;
					}
				}
				//更新路由表
				if(minCost < dv[nbrNum].dvEntry[i].cost)
				{
					dv[nbrNum].dvEntry[i].cost = minCost;
					pthread_mutex_lock(routingtable_mutex);
					routingtable_setnextnode(routingtable, destID, minID);
					pthread_mutex_unlock(routingtable_mutex);
				}
			}
			pthread_mutex_unlock(dv_mutex);
		}
	}
	return NULL;
}

//这个函数终止SIP进程, 当SIP进程收到信号SIGINT时会调用这个函数. 
//它关闭所有连接, 释放所有动态分配的内存.
void sip_stop() {
	if(son_conn > 0)
		close(son_conn);
	if(stcp_conn > 0)
		close(stcp_conn);
	nbrcosttable_destroy(nct);
	dvtable_destroy(dv);
	routingtable_destroy(routingtable);
	pthread_mutex_destroy(dv_mutex);
	pthread_mutex_destroy(routingtable_mutex);
	exit(0);
}

//这个函数打开端口SIP_PORT并等待来自本地STCP进程的TCP连接.
//在连接建立后, 这个函数从STCP进程处持续接收包含段及其目的节点ID的sendseg_arg_t. 
//接收的段被封装进数据报(一个段在一个数据报中), 然后使用son_sendpkt发送该报文到下一跳. 下一跳节点ID提取自路由表.
//当本地STCP进程断开连接时, 这个函数等待下一个STCP进程的连接.
void waitSTCP() {
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
	my_addr.sin_port = htons(SIP_PORT);
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
	stcp_conn = accept(sockfd, NULL, NULL);
	if (stcp_conn == -1)
	{
		perror("accept");
		exit(1);
	}
	// 接收来自STCP进程的数据
	while(1)
	{
		seg_t seg;
		int destNodeID;
		int nextNodeID;
		res = getsegToSend(stcp_conn, &destNodeID, &seg);
		if (res == -1)
		{
			perror("getsegToSend");
			close(stcp_conn);
			stcp_conn = -1;
			goto reaccept;
		}
		// 封装数据报
		sip_pkt_t pkt;
		pkt.header.src_nodeID = myID;
		pkt.header.dest_nodeID = destNodeID;
		pkt.header.type = SIP;
		pkt.header.length = sizeof(seg_t);
		memcpy(pkt.data, &seg, sizeof(seg_t));
		// 发送数据报
		pthread_mutex_lock(routingtable_mutex);
		nextNodeID = routingtable_getnextnode(routingtable, destNodeID);
		pthread_mutex_unlock(routingtable_mutex);
		res = son_sendpkt(nextNodeID, &pkt, son_conn);
		if (res == -1)
		{
			perror("son_sendpkt");
			sip_stop();
		}
	}
	return;
}

int main(int argc, char *argv[]) {
	printf("SIP layer is starting, pls wait...\n");

	//初始化全局变量
	topology_init();
	nct = nbrcosttable_create();
	dv = dvtable_create();
	dv_mutex = (pthread_mutex_t*)malloc(sizeof(pthread_mutex_t));
	pthread_mutex_init(dv_mutex,NULL);
	routingtable = routingtable_create();
	routingtable_mutex = (pthread_mutex_t*)malloc(sizeof(pthread_mutex_t));
	pthread_mutex_init(routingtable_mutex,NULL);
	son_conn = -1;
	stcp_conn = -1;

	nbrcosttable_print(nct);
	dvtable_print(dv);
	routingtable_print(routingtable);

	//注册用于终止进程的信号句柄
	signal(SIGINT, sip_stop);

	//连接到本地SON进程 
	son_conn = connectToSON();
	if(son_conn<0) {
		printf("can't connect to SON process\n");
		exit(1);		
	}
	
	//启动线程处理来自SON进程的进入报文 
	pthread_t pkt_handler_thread; 
	pthread_create(&pkt_handler_thread,NULL,pkthandler,(void*)0);

	//启动路由更新线程 
	pthread_t routeupdate_thread;
	pthread_create(&routeupdate_thread,NULL,routeupdate_daemon,(void*)0);	

	printf("SIP layer is started...\n");
	printf("waiting for routes to be established\n");
	sleep(SIP_WAITTIME);
	routingtable_print(routingtable);

	//等待来自STCP进程的连接
	printf("waiting for connection from STCP process\n");
	waitSTCP(); 

}


