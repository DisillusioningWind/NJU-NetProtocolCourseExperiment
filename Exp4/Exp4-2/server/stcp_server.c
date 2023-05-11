// 文件名: stcp_server.c
// 描述: 这个文件包含服务器STCP套接字接口定义. 你需要实现所有这些接口.

#include <stdlib.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <stdio.h>
#include <time.h>
#include <pthread.h>
#include "stcp_server.h"
#include "../common/constants.h"

int sip_conn;                                         //重叠网络层TCP套接字描述符
server_tcb_t *tcb_list[MAX_TRANSPORT_CONNECTIONS];    //服务器端TCB表
pthread_mutex_t st_mutex = PTHREAD_MUTEX_INITIALIZER; //TCB表访问互斥锁
pthread_cond_t st_cond = PTHREAD_COND_INITIALIZER;    //TCB表访问条件变量

/// @brief 初始化服务器TCB表
/// @param conn 重叠网络TCP套接字描述符
void stcp_server_init(int conn)
{
  for (int i = 0; i < MAX_TRANSPORT_CONNECTIONS; i++)
  {
    tcb_list[i] = NULL;
  }
  sip_conn = conn;
  pthread_t thread_seg, thread_timer;
  pthread_create(&thread_seg, NULL, seghandler, NULL);
  pthread_create(&thread_timer, NULL, timer_thread, NULL);
}

/// @brief 创建STCP服务器套接字
/// @param server_port 服务器端口号
/// @retval >=0 创建成功，返回服务器TCB表中的条目索引
/// @retval -1 创建失败
int stcp_server_sock(unsigned int server_port)
{
  for (int i = 0; i < MAX_TRANSPORT_CONNECTIONS; i++)
  {
    if (tcb_list[i] == NULL)
    {
      tcb_list[i] = (server_tcb_t *)malloc(sizeof(server_tcb_t));
      tcb_list[i]->server_portNum = server_port;
      tcb_list[i]->client_portNum = 0;
      tcb_list[i]->state = CLOSED;
      tcb_list[i]->expect_seqNum = 0;
      tcb_list[i]->recvBuf = (char *)malloc(sizeof(char) * RECEIVE_BUF_SIZE);
      tcb_list[i]->usedBufLen = 0;
      tcb_list[i]->bufMutex = (pthread_mutex_t *)malloc(sizeof(pthread_mutex_t));
      pthread_mutex_init(tcb_list[i]->bufMutex, NULL);
      return i;
    }
  }
  return -1;
}

/// @brief 监听来自STCP客户端的连接
/// @param sockfd STCP套接字描述符
/// @return 1 连接成功
int stcp_server_accept(int sockfd)
{
  pthread_mutex_lock(&st_mutex);
  tcb_list[sockfd]->state = LISTENING;
  while (tcb_list[sockfd]->state != CONNECTED)
  {
    pthread_cond_wait(&st_cond, &st_mutex);
    if (tcb_list[sockfd]->state == CONNECTED)
    {
      pthread_mutex_unlock(&st_mutex);
      printf("Server connect success, client port:%d, server port:%d\n", tcb_list[sockfd]->client_portNum, tcb_list[sockfd]->server_portNum);
      break;
    }
  }
  return 1;
}

// 接收来自STCP客户端的数据. 请回忆STCP使用的是单向传输, 数据从客户端发送到服务器端.
// 信号/控制信息(如SYN, SYNACK等)则是双向传递. 这个函数每隔RECVBUF_ROLLING_INTERVAL时间
// 就查询接收缓冲区, 直到等待的数据到达, 它然后存储数据并返回1. 如果这个函数失败, 则返回-1.
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//
int stcp_server_recv(int sockfd, void* buf, unsigned int length)
{
  return 0;
}

/// @brief 关闭STCP服务器套接字
/// @param sockfd STCP套接字描述符
/// @retval 1 关闭成功
/// @retval -1 关闭失败
int stcp_server_close(int sockfd)
{
  if (tcb_list[sockfd]->state == CLOSED)
  {
    int cli_port = tcb_list[sockfd]->client_portNum;
    pthread_mutex_lock(&st_mutex);
    free(tcb_list[sockfd]->bufMutex);
    free(tcb_list[sockfd]->recvBuf);
    free(tcb_list[sockfd]);
    tcb_list[sockfd] = NULL;
    pthread_mutex_unlock(&st_mutex);
    printf("Server connect close, client port:%d\n", cli_port);
    return 1;
  }
  return -1;
}

/// @brief 处理接收到的STCP段
void* seghandler(void* arg)
{
  struct timeval rtv, tv;
  tv.tv_sec = CLOSEWAIT_TIMEOUT;
  tv.tv_usec = 0;
  while (1)
  {
    seg_t seg;
    int ret;
    ret = sip_recvseg(sip_conn, &seg);
    if (ret == -1)
    {
      //重叠网络连接关闭或出错
      printf("Server sip connect closed or error\n");
      return NULL;
    }
    else if (ret == 1)
    {
      //报文丢失
      continue;
    }
    //接收成功
    int sockfd = -1;
    for (int i = 0; i < MAX_TRANSPORT_CONNECTIONS; i++)
    {
      if (tcb_list[i] != NULL && tcb_list[i]->server_portNum == seg.header.dest_port)
      {
        sockfd = i;
        break;
      }
    }
    // printf("sockfd = %d\n", sockfd);
    if (sockfd == -1)
    {
      printf("Server sockfd error\n");
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

/// @brief 定时器线程，处理CLOSEWAIT状态的超时
void *timer_thread(void *arg)
{
  struct timeval rtv, tv;
  tv.tv_sec = CLOSEWAIT_TIMEOUT;
  tv.tv_usec = 0;
  while (1)
  {
    rtv = tv;
    if (select(0, NULL, NULL, NULL, &rtv) == 0)
    {
      //CLOSEWAIT超时
      pthread_mutex_lock(&st_mutex);
      for (int i = 0; i < MAX_TRANSPORT_CONNECTIONS; i++)
      {
        if (tcb_list[i] != NULL && tcb_list[i]->state == CLOSEWAIT)
        {
          tcb_list[i]->state = CLOSED;
          printf("Server sockfd %d closewait timeout\n", i);
        }
      }
      pthread_mutex_unlock(&st_mutex);
    }
    else
    {
      printf("Server timer_thread select error\n");
      return NULL;
    }
  }
  return NULL;
}