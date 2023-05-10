#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/time.h>
#include <time.h>
#include <pthread.h>
#include "stcp_server.h"
#include "../common/constants.h"

/*面向应用层的接口*/

//
//
//  我们在下面提供了每个函数调用的原型定义和细节说明, 但这些只是指导性的, 你完全可以根据自己的想法来设计代码.
//
//  注意: 当实现这些函数时, 你需要考虑FSM中所有可能的状态, 这可以使用switch语句来实现.
//
//  目标: 你的任务就是设计并实现下面的函数原型.
//

// stcp服务器初始化
//
// 这个函数初始化TCB表, 将所有条目标记为NULL. 它还针对重叠网络TCP套接字描述符conn初始化一个STCP层的全局变量,
// 该变量作为sip_sendseg和sip_recvseg的输入参数. 最后, 这个函数启动seghandler线程来处理进入的STCP段.
// 服务器只有一个seghandler.
//

server_tcb_t *tcb_list[MAX_TRANSPORT_CONNECTIONS];
int sip_conn;
pthread_mutex_t st_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t st_cond = PTHREAD_COND_INITIALIZER;

void stcp_server_init(int conn) {
	for (int i = 0; i < MAX_TRANSPORT_CONNECTIONS; i++) {
		tcb_list[i] = NULL;
	}
	sip_conn = conn;
	pthread_t thread;
	pthread_create(&thread, NULL, seghandler, NULL);
}

// 创建服务器套接字
//
// 这个函数查找服务器TCB表以找到第一个NULL条目, 然后使用malloc()为该条目创建一个新的TCB条目.
// 该TCB中的所有字段都被初始化, 例如, TCB state被设置为CLOSED, 服务器端口被设置为函数调用参数server_port. 
// TCB表中条目的索引应作为服务器的新套接字ID被这个函数返回, 它用于标识服务器的连接. 
// 如果TCB表中没有条目可用, 这个函数返回-1.

int stcp_server_sock(unsigned int server_port) {
	for (int i = 0; i < MAX_TRANSPORT_CONNECTIONS; i++) {
		if (tcb_list[i] == NULL) {
			tcb_list[i] = (server_tcb_t *)malloc(sizeof(server_tcb_t));
			tcb_list[i]->server_nodeID = 0;
			tcb_list[i]->server_portNum = server_port;
			tcb_list[i]->client_nodeID = 0;
			tcb_list[i]->client_portNum = 0;
			tcb_list[i]->state = CLOSED; 
			tcb_list[i]->expect_seqNum = 0;
			tcb_list[i]->recvBuf = NULL;
			tcb_list[i]->usedBufLen = 0;
			tcb_list[i]->bufMutex = (pthread_mutex_t *)malloc(sizeof(pthread_mutex_t));
			pthread_mutex_init(tcb_list[i]->bufMutex, NULL);
			return i;
		}
	}
	return -1;
}

// 接受来自STCP客户端的连接
//
// 这个函数使用sockfd获得TCB指针, 并将连接的state转换为LISTENING. 它然后进入忙等待(busy wait)直到TCB状态转换为CONNECTED 
// (当收到SYN时, seghandler会进行状态的转换). 该函数在一个无穷循环中等待TCB的state转换为CONNECTED,  
// 当发生了转换时, 该函数返回1. 你可以使用不同的方法来实现这种阻塞等待.
//

int stcp_server_accept(int sockfd) {
	pthread_mutex_lock(&st_mutex);
	tcb_list[sockfd]->state = LISTENING;
	while (tcb_list[sockfd]->state != CONNECTED) {
		pthread_cond_wait(&st_cond, &st_mutex);
		if(tcb_list[sockfd]->state == CONNECTED) {
			pthread_mutex_unlock(&st_mutex);
			printf("Server connect success\n");
			break;
		}
	}
	return 1;
}

// 接收来自STCP客户端的数据
//
// 这个函数接收来自STCP客户端的数据. 你不需要在本实验中实现它.
//
int stcp_server_recv(int sockfd, void* buf, unsigned int length) {
	return 1;
}

// 关闭STCP服务器
//
// 这个函数调用free()释放TCB条目. 它将该条目标记为NULL, 成功时(即位于正确的状态)返回1,
// 失败时(即位于错误的状态)返回-1.
//

int stcp_server_close(int sockfd) {
	if (tcb_list[sockfd]->state == CLOSED) {
		pthread_mutex_lock(&st_mutex);
		free(tcb_list[sockfd]->bufMutex);
		free(tcb_list[sockfd]);
		tcb_list[sockfd] = NULL;
		pthread_mutex_unlock(&st_mutex);
		printf("Server connect close\n");
		return 1;
	}
	return -1;
}

// 处理进入段的线程
//
// 这是由stcp_server_init()启动的线程. 它处理所有来自客户端的进入数据. seghandler被设计为一个调用sip_recvseg()的无穷循环, 
// 如果sip_recvseg()失败, 则说明重叠网络连接已关闭, 线程将终止. 根据STCP段到达时连接所处的状态, 可以采取不同的动作.
// 请查看服务端FSM以了解更多细节.
//

void *seghandler(void* arg) {
	//fd_set rset, set;
	struct timeval rtv, tv;
	//FD_ZERO(&set);
	//FD_SET(sip_conn, &set);
	rtv.tv_sec = CLOSEWAIT_TIMEOUT;
	rtv.tv_usec = 0;

	while (1) {
		seg_t seg;
		int ret;

		ret = sip_recvseg(sip_conn, &seg);
		if (ret == -1)
		{
			printf("recv error in recvmeg\n");
			return NULL;
		}
		else if(ret == -2)
		{
			//客户端关闭
			while (1)
			{
				tv = rtv;
				int ready = select(0, NULL, NULL, NULL, &tv);
				// CLOSEWAIT超时
				if (ready == 0)
				{
					pthread_mutex_lock(&st_mutex);
					for (int i = 0; i < MAX_TRANSPORT_CONNECTIONS; i++)
					{
						if (tcb_list[i] != NULL && tcb_list[i]->state == CLOSEWAIT)
						{
							tcb_list[i]->state = CLOSED;
							//printf("Server sockfd %d closewait timeout\n", i);
						}
					}
					pthread_mutex_unlock(&st_mutex);
					continue;
				}
			}
			break;
		}
		else if (ret == 1)
		{
			//模拟报文丢失
			continue;
		}
		int sockfd = -1;
		for (int i = 0; i < MAX_TRANSPORT_CONNECTIONS; i++) {
			if (tcb_list[i] != NULL && tcb_list[i]->server_portNum == seg.header.dest_port) {
				sockfd = i;
				break;
			}
		}
		// printf("sockfd = %d\n", sockfd);
		if (sockfd == -1) {
			continue;
		}
		pthread_mutex_lock(&st_mutex);
		switch (tcb_list[sockfd]->state)
		{
		case CLOSED:
			break;
		case LISTENING:
			if (seg.header.type == SYN)
			{
				printf("Server recv SYN\n");
				tcb_list[sockfd]->client_portNum = seg.header.src_port;
				tcb_list[sockfd]->expect_seqNum = seg.header.seq_num + 1;
				tcb_list[sockfd]->state = CONNECTED;
				pthread_cond_signal(&st_cond);
				seg.header.src_port = tcb_list[sockfd]->server_portNum;
				seg.header.dest_port = tcb_list[sockfd]->client_portNum;
				seg.header.type = SYNACK;
				sip_sendseg(sip_conn, &seg);
				printf("Server send SYNACK\n");
			}
			break;
		case CONNECTED:
			if (seg.header.type == FIN)
			{
				printf("Server recv FIN\n");
				tcb_list[sockfd]->state = CLOSEWAIT;
				seg.header.src_port = tcb_list[sockfd]->server_portNum;
				seg.header.dest_port = tcb_list[sockfd]->client_portNum;
				seg.header.type = FINACK;
				sip_sendseg(sip_conn, &seg);
				printf("Server send FINACK\n");
			}
			else if (seg.header.type == SYN)
			{
				seg.header.src_port = tcb_list[sockfd]->server_portNum;
				seg.header.dest_port = tcb_list[sockfd]->client_portNum;
				seg.header.type = SYNACK;
				sip_sendseg(sip_conn, &seg);
			}
			break;
		case CLOSEWAIT:
			if (seg.header.type == FIN)
			{
				printf("Server recv FIN\n");
				seg.header.src_port = tcb_list[sockfd]->server_portNum;
				seg.header.dest_port = tcb_list[sockfd]->client_portNum;
				seg.header.type = FINACK;
				sip_sendseg(sip_conn, &seg);
				printf("Server send FINACK\n");
			}
			break;
		}
		pthread_mutex_unlock(&st_mutex);
	}
	return NULL;
}
