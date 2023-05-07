#include <stdlib.h>
#include <stdio.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <time.h>
#include <pthread.h>
#include "stcp_client.h"

/*面向应用层的接口*/

//
//  我们在下面提供了每个函数调用的原型定义和细节说明, 但这些只是指导性的, 你完全可以根据自己的想法来设计代码.
//
//  注意: 当实现这些函数时, 你需要考虑FSM中所有可能的状态, 这可以使用switch语句来实现.
//
//  目标: 你的任务就是设计并实现下面的函数原型.
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

// stcp客户端初始化
//
// 这个函数初始化TCB表, 将所有条目标记为NULL.  
// 它还针对重叠网络TCP套接字描述符conn初始化一个STCP层的全局变量, 该变量作为sip_sendseg和sip_recvseg的输入参数.
// 最后, 这个函数启动seghandler线程来处理进入的STCP段. 客户端只有一个seghandler.
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//

client_tcb_t *tcb_list[MAX_TRANSPORT_CONNECTIONS];
int sip_conn;
pthread_mutex_t st_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t st_cond = PTHREAD_COND_INITIALIZER;

void stcp_client_init(int conn) {
  for (int i = 0; i < MAX_TRANSPORT_CONNECTIONS; i++) {
    tcb_list[i] = NULL;
  }
  sip_conn = conn;
  pthread_t thread;
  pthread_create(&thread, NULL, seghandler, NULL);
  return;
}

// 创建一个客户端TCB条目, 返回套接字描述符
//
// 这个函数查找客户端TCB表以找到第一个NULL条目, 然后使用malloc()为该条目创建一个新的TCB条目.
// 该TCB中的所有字段都被初始化. 例如, TCB state被设置为CLOSED，客户端端口被设置为函数调用参数client_port. 
// TCB表中条目的索引号应作为客户端的新套接字ID被这个函数返回, 它用于标识客户端的连接. 
// 如果TCB表中没有条目可用, 这个函数返回-1.
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//

int stcp_client_sock(unsigned int client_port) {
  for (int i = 0; i < MAX_TRANSPORT_CONNECTIONS; i++) {
    if (tcb_list[i] == NULL) {
      client_tcb_t *tcb = (client_tcb_t *)malloc(sizeof(client_tcb_t));
      tcb->server_nodeID = 0;
      tcb->server_portNum = 0;
      tcb->client_nodeID = 0;
      tcb->client_portNum = client_port;
      tcb->state = CLOSED;
      tcb->next_seqNum = 0;
      tcb->bufMutex = (pthread_mutex_t *)malloc(sizeof(pthread_mutex_t));
      pthread_mutex_init(tcb->bufMutex, NULL);
      tcb->sendBufHead = NULL;
      tcb->sendBufunSent = NULL;
      tcb->sendBufTail = NULL;
      tcb->unAck_segNum = 0;
      tcb_list[i] = tcb;
      return i;
    }
  }
  return 0;
}

// 连接STCP服务器
//
// 这个函数用于连接服务器. 它以套接字ID和服务器的端口号作为输入参数. 套接字ID用于找到TCB条目.  
// 这个函数设置TCB的服务器端口号,  然后使用sip_sendseg()发送一个SYN段给服务器.  
// 在发送了SYN段之后, 一个定时器被启动. 如果在SYNSEG_TIMEOUT时间之内没有收到SYNACK, SYN 段将被重传. 
// 如果收到了, 就返回1. 否则, 如果重传SYN的次数大于SYN_MAX_RETRY, 就将state转换到CLOSED, 并返回-1.
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//

int stcp_client_connect(int sockfd, unsigned int server_port) {
  if(sockfd < 0 || sockfd >= MAX_TRANSPORT_CONNECTIONS || tcb_list[sockfd] == NULL) {
    printf("stcp_client_connect: sockfd error\n");
    return -1;
  }
  tcb_list[sockfd]->server_portNum = server_port;
  seg_t seg;
  seg.header.src_port = tcb_list[sockfd]->client_portNum;
  seg.header.dest_port = tcb_list[sockfd]->server_portNum;
  seg.header.seq_num = tcb_list[sockfd]->next_seqNum;
  seg.header.ack_num = 0;
  seg.header.length = 0;
  seg.header.type = SYN;
  seg.header.rcv_win = 0;
  seg.header.checksum = 0;

  if (sip_sendseg(sip_conn, &seg) < 0) {
    printf("stcp_client_connect: sip_sendseg error\n");
    return -1;
  }
  tcb_list[sockfd]->state = SYNSENT;
  int retry = 0;
  struct timeval tv, rtv;
  rtv.tv_sec = 0;
  rtv.tv_usec = SYN_TIMEOUT / 1000;//1us = 1000ns
  while (retry < SYN_MAX_RETRY)
  {
    tv = rtv;
    select(0, NULL, NULL, NULL, &tv);
    pthread_mutex_lock(tcb_list[sockfd]->bufMutex);
    if (tcb_list[sockfd]->state == CONNECTED) {
      pthread_mutex_unlock(tcb_list[sockfd]->bufMutex);
      return 1;
    }
    if (sip_sendseg(sip_conn, &seg) < 0) {
      printf("stcp_client_connect: sip_sendseg error\n");
      return -1;
    }
    pthread_mutex_unlock(tcb_list[sockfd]->bufMutex);
    retry++;
  }
  tcb_list[sockfd]->state = CLOSED;
  return -1;
}

// 发送数据给STCP服务器
//
// 这个函数发送数据给STCP服务器. 你不需要在本实验中实现它。
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//

int stcp_client_send(int sockfd, void* data, unsigned int length) {
	return 1;
}

// 断开到STCP服务器的连接
//
// 这个函数用于断开到服务器的连接. 它以套接字ID作为输入参数. 套接字ID用于找到TCB表中的条目.  
// 这个函数发送FIN segment给服务器. 在发送FIN之后, state将转换到FINWAIT, 并启动一个定时器.
// 如果在最终超时之前state转换到CLOSED, 则表明FINACK已被成功接收. 否则, 如果在经过FIN_MAX_RETRY次尝试之后,
// state仍然为FINWAIT, state将转换到CLOSED, 并返回-1.
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//

int stcp_client_disconnect(int sockfd) {
  if(sockfd < 0 || sockfd >= MAX_TRANSPORT_CONNECTIONS || tcb_list[sockfd] == NULL) {
    printf("stcp_client_disconnect: sockfd error\n");
    return -1;
  }
  seg_t seg;
  seg.header.src_port = tcb_list[sockfd]->client_portNum;
  seg.header.dest_port = tcb_list[sockfd]->server_portNum;
  seg.header.seq_num = tcb_list[sockfd]->next_seqNum;
  seg.header.ack_num = 0;
  seg.header.length = 0;
  seg.header.type = FIN;
  seg.header.rcv_win = 0;
  seg.header.checksum = 0;

  if (sip_sendseg(sip_conn, &seg) < 0) {
    printf("stcp_client_disconnect: sip_sendseg error\n");
    return -1;
  }
  tcb_list[sockfd]->state = FINWAIT;
  int retry = 0;
  struct timeval tv, rtv;
  rtv.tv_sec = 0;
  rtv.tv_usec = FIN_TIMEOUT / 1000;//1us = 1000ns
  while (retry < FIN_MAX_RETRY)
  {
    tv = rtv;
    select(0, NULL, NULL, NULL, &tv);
    pthread_mutex_lock(tcb_list[sockfd]->bufMutex);
    if (tcb_list[sockfd]->state == CLOSED) {
      pthread_mutex_unlock(tcb_list[sockfd]->bufMutex);
      return 1;
    }
    if (sip_sendseg(sip_conn, &seg) < 0) {
      printf("stcp_client_disconnect: sip_sendseg error\n");
      return -1;
    }
    pthread_mutex_unlock(tcb_list[sockfd]->bufMutex);
    retry++;
  }
  tcb_list[sockfd]->state = CLOSED;
  return -1;
}

// 关闭STCP客户
//
// 这个函数调用free()释放TCB条目. 它将该条目标记为NULL, 成功时(即位于正确的状态)返回1,
// 失败时(即位于错误的状态)返回-1.
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//

int stcp_client_close(int sockfd) {
  if(sockfd < 0 || sockfd >= MAX_TRANSPORT_CONNECTIONS || tcb_list[sockfd] == NULL) {
    printf("stcp_client_close: sockfd error\n");
    return -1;
  }
  if (tcb_list[sockfd]->state == CLOSED) {
    free(tcb_list[sockfd]->bufMutex);
    free(tcb_list[sockfd]);
    tcb_list[sockfd] = NULL;
    return 1;
  }
  return -1;
}

// 处理进入段的线程
//
// 这是由stcp_client_init()启动的线程. 它处理所有来自服务器的进入段. 
// seghandler被设计为一个调用sip_recvseg()的无穷循环. 如果sip_recvseg()失败, 则说明重叠网络连接已关闭,
// 线程将终止. 根据STCP段到达时连接所处的状态, 可以采取不同的动作. 请查看客户端FSM以了解更多细节.
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//

void *seghandler(void* arg) {
  fd_set rset, set;
  struct timeval rtv, tv;
  int res;
  FD_ZERO(&set);
  FD_SET(sip_conn, &set);
  rtv.tv_sec = 0;
  rtv.tv_usec = SYN_TIMEOUT / 1000;//1us = 1000ns

  while (1) {
    tv = rtv;
    rset = set;
    res = select(sip_conn + 1, &rset, NULL, NULL, &tv);
    if (res < 0) {
      printf("seghandler: select error\n");
      return NULL;
    }
    else if (res == 0) {
      continue;
    }
    if (FD_ISSET(sip_conn, &rset)) {
      seg_t seg;
      if (sip_recvseg(sip_conn, &seg) < 0) {
        printf("seghandler: sip_recvseg error\n");
        return NULL;
      }
      int sockfd = -1;
      for (int i = 0; i < MAX_TRANSPORT_CONNECTIONS; i++) {
        if (tcb_list[i] != NULL && tcb_list[i]->client_portNum == seg.header.dest_port) {
          sockfd = i;
          break;
        }
      }
      if (sockfd < 0) {
        printf("seghandler: get_sockfd error\n");
        return NULL;
      }
      switch(tcb_list[sockfd]->state) {
        case CLOSED:
          break;
        case SYNSENT:
          if (seg.header.type == SYNACK) {
            tcb_list[sockfd]->state = CONNECTED;
            tcb_list[sockfd]->server_portNum = seg.header.src_port;
            printf("SYNACK received\n");
          }
          break;
        case CONNECTED:
          //nothing to do
          break;
        case FINWAIT:
          if (seg.header.type == FINACK) {
            tcb_list[sockfd]->state = CLOSED;
            printf("FINACK received\n");
          }
          break;
      }
    }
  }
  return 0;
}



