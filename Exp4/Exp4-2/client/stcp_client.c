#include <stdlib.h>
#include <stdio.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <time.h>
#include <pthread.h>
#include <string.h>
#include "stcp_client.h"

// #define CLOCK_REALTIME 0

int sip_conn;                                         //重叠网络层TCP套接字描述符
client_tcb_t *tcb_list[MAX_TRANSPORT_CONNECTIONS];    //客户端TCB表
pthread_mutex_t st_mutex = PTHREAD_MUTEX_INITIALIZER; //TCB表互斥锁
pthread_cond_t st_cond = PTHREAD_COND_INITIALIZER;    //TCB表条件变量

/// @brief 初始化客户端TCB表
/// @param conn 重叠网络层TCP套接字描述符
void stcp_client_init(int conn)
{
  for (int i = 0; i < MAX_TRANSPORT_CONNECTIONS; i++)
  {
    tcb_list[i] = NULL;
  }
  sip_conn = conn;
  pthread_t thread;
  pthread_create(&thread, NULL, seghandler, NULL);
}

/// @brief 创建STCP客户端套接字
/// @param client_port 客户端端口号
/// @retval >=0 创建成功，返回TCB表中的条目索引
/// @retval -1 创建失败
int stcp_client_sock(unsigned int client_port)
{
  for (int i = 0; i < MAX_TRANSPORT_CONNECTIONS; i++)
  {
    if (tcb_list[i] == NULL)
    {
      client_tcb_t *tcb = (client_tcb_t *)malloc(sizeof(client_tcb_t));
      tcb->server_portNum = 0;
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
  return -1;
}

/// @brief 连接STCP服务器
/// @param sockfd 套接字ID
/// @param server_port 服务器端口号
/// @retval 1 连接成功
/// @retval -1 连接失败
int stcp_client_connect(int sockfd, unsigned int server_port)
{
  if (sockfd < 0 || sockfd >= MAX_TRANSPORT_CONNECTIONS || tcb_list[sockfd] == NULL)
  {
    printf("stcp_client_connect: sockfd error\n");
    return -1;
  }
  tcb_list[sockfd]->server_portNum = server_port;
  seg_t seg;
  seg.header.src_port = tcb_list[sockfd]->client_portNum;
  seg.header.dest_port = tcb_list[sockfd]->server_portNum;
  seg.header.type = SYN;
  seg.header.length = 0;
  seg.header.seq_num = tcb_list[sockfd]->next_seqNum;
  seg.header.checksum = checksum(&seg);

  if (sip_sendseg(sip_conn, &seg) < 0)
  {
    printf("stcp_client_connect: sip_sendseg error\n");
    return -1;
  }
  printf("Client send SYN\n");

  tcb_list[sockfd]->state = SYNSENT;
  int retry = 0;
  struct timeval tv, rtv;
  rtv.tv_sec = 0;
  rtv.tv_usec = SYN_TIMEOUT / 1000; // 1us = 1000ns
  while (retry < SYN_MAX_RETRY)
  {
    tv = rtv;
    select(0, NULL, NULL, NULL, &tv);
    pthread_mutex_lock(&st_mutex);
    if (tcb_list[sockfd]->state == CONNECTED)
    {
      pthread_mutex_unlock(&st_mutex);
      return 1;
    }
    if (sip_sendseg(sip_conn, &seg) < 0)
    {
      printf("stcp_client_connect: sip_sendseg error\n");
      return -1;
    }
    printf("Client send SYN\n");
    pthread_mutex_unlock(&st_mutex);
    retry++;
  }
  tcb_list[sockfd]->state = CLOSED;
  printf("Client retry exceed\n");
  return -1;
}

/// @brief 向STCP服务器发送数据
/// @param sockfd 套接字ID
/// @param data 发送数据缓冲区
/// @param length 发送数据长度
/// @retval 1 发送成功
/// @retval -1 发送失败
int stcp_client_send(int sockfd, void* data, unsigned int length)
{
  if (sockfd < 0 || sockfd >= MAX_TRANSPORT_CONNECTIONS || tcb_list[sockfd] == NULL)
  {
    printf("stcp_client_send: sockfd error\n");
    return -1;
  }
  printf("Client send data in\n");
  printf("lengt: %d\n", length);
  
  pthread_mutex_lock(tcb_list[sockfd]->bufMutex);
  segBuf_t *segBuf = (segBuf_t *)malloc(sizeof(segBuf_t));
  segBuf->seg.header.src_port = tcb_list[sockfd]->client_portNum;
  segBuf->seg.header.dest_port = tcb_list[sockfd]->server_portNum;
  segBuf->seg.header.type = DATA;
  segBuf->seg.header.length = length;
  segBuf->seg.header.seq_num = tcb_list[sockfd]->next_seqNum;
  memcpy(segBuf->seg.data, data, length);
  segBuf->seg.header.checksum = checksum(&segBuf->seg);
  segBuf->sentTime = get_time_now();
  segBuf->next = NULL;
  if (tcb_list[sockfd]->sendBufHead == NULL)
  {
    tcb_list[sockfd]->sendBufHead = segBuf;
    tcb_list[sockfd]->sendBufunSent = segBuf;
    tcb_list[sockfd]->sendBufTail = segBuf;
    pthread_t thread;
    pthread_create(&thread, NULL, sendBuf_timer, (void *)sockfd);
  }
  else
  {
    tcb_list[sockfd]->sendBufTail->next = segBuf;
    tcb_list[sockfd]->sendBufTail = segBuf;
    if(tcb_list[sockfd]->sendBufunSent == NULL)
      tcb_list[sockfd]->sendBufunSent = segBuf;
  }
  pthread_mutex_unlock(tcb_list[sockfd]->bufMutex);
  tcb_list[sockfd]->next_seqNum += length;
  //从发送缓冲区中第一个未发送段开始发送，直到已发送但未被确认数据段的数目到达GBN_WINDOW
  printf("unAck_segNum: %d\n", tcb_list[sockfd]->unAck_segNum);
  while(tcb_list[sockfd]->unAck_segNum < GBN_WINDOW)
  {
    pthread_mutex_lock(tcb_list[sockfd]->bufMutex);
    if (tcb_list[sockfd]->sendBufunSent == NULL)
    {
      pthread_mutex_unlock(tcb_list[sockfd]->bufMutex);
      break;
    }
    seg_t seg = tcb_list[sockfd]->sendBufunSent->seg;
    pthread_mutex_unlock(tcb_list[sockfd]->bufMutex);
    printf("seq_num: %d\n", seg.header.seq_num);
    if (sip_sendseg(sip_conn, &seg) < 0)
    {
      printf("stcp_client_send: sip_sendseg error\n");
      return -1;
    }
    printf("Client send DATA\n");
    tcb_list[sockfd]->sendBufunSent = tcb_list[sockfd]->sendBufunSent->next;
    tcb_list[sockfd]->unAck_segNum++;
  }
  printf("Client send data out\n");
  return 1;
}

/// @brief 关闭STCP连接.
/// @param sockfd 套接字ID
/// @retval 1 关闭成功
/// @retval -1 关闭失败
int stcp_client_disconnect(int sockfd)
{
  if (sockfd < 0 || sockfd >= MAX_TRANSPORT_CONNECTIONS || tcb_list[sockfd] == NULL)
  {
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
  seg.header.checksum = checksum(&seg);

  if (sip_sendseg(sip_conn, &seg) < 0)
  {
    printf("stcp_client_disconnect: sip_sendseg error\n");
    return -1;
  }
  printf("Client send FIN\n");

  tcb_list[sockfd]->state = FINWAIT;
  int retry = 0;
  struct timeval tv, rtv;
  rtv.tv_sec = 0;
  rtv.tv_usec = FIN_TIMEOUT / 1000; // 1us = 1000ns
  while (retry < FIN_MAX_RETRY)
  {
    tv = rtv;
    select(0, NULL, NULL, NULL, &tv);
    pthread_mutex_lock(&st_mutex);
    if (tcb_list[sockfd]->state == CLOSED)
    {
      pthread_mutex_unlock(&st_mutex);
      printf("Client connect closed\n");
      return 1;
    }
    if (sip_sendseg(sip_conn, &seg) < 0)
    {
      printf("stcp_client_disconnect: sip_sendseg error\n");
      return -1;
    }
    printf("Client send FIN\n");
    pthread_mutex_unlock(&st_mutex);
    retry++;
  }
  tcb_list[sockfd]->state = CLOSED;
  printf("Client FIN retry exceed\n");
  printf("Client connect closed\n");
  return -1;
}

/// @brief 关闭STCP客户端
/// @param sockfd 套接字ID
/// @retval 1 关闭成功
/// @retval -1 关闭失败
int stcp_client_close(int sockfd)
{
  if (sockfd < 0 || sockfd >= MAX_TRANSPORT_CONNECTIONS || tcb_list[sockfd] == NULL)
  {
    printf("stcp_client_close: sockfd error\n");
    return -1;
  }
  if (tcb_list[sockfd]->state == CLOSED)
  {
    pthread_mutex_lock(&st_mutex);
    segBuf_t *segBuf = tcb_list[sockfd]->sendBufHead;
    while (segBuf != NULL)
    {
      tcb_list[sockfd]->sendBufHead = tcb_list[sockfd]->sendBufHead->next;
      free(segBuf);
      segBuf = tcb_list[sockfd]->sendBufHead;
    }
    free(tcb_list[sockfd]->bufMutex);
    free(tcb_list[sockfd]);
    tcb_list[sockfd] = NULL;
    pthread_mutex_unlock(&st_mutex);
    return 1;
  }
  return -1;
}

/// @brief 处理接收到的STCP段
void *seghandler(void* arg)
{
  fd_set rset, set;
  struct timeval rtv, tv;
  int res;
  FD_ZERO(&set);
  FD_SET(sip_conn, &set);
  rtv.tv_sec = 0;
  rtv.tv_usec = SYN_TIMEOUT / 1000; // 1us = 1000ns

  while (1)
  {
    tv = rtv;
    rset = set;
    res = select(sip_conn + 1, &rset, NULL, NULL, NULL);
    if (res < 0)
    {
      printf("seghandler: select error\n");
      return NULL;
    }
    else if (res == 0)
    {
      continue;
    }
    if (FD_ISSET(sip_conn, &rset))
    {
      seg_t seg;
      int res = sip_recvseg(sip_conn, &seg);
      if (res == -1)
      {
        printf("Client sip connect closed\n");
        return NULL;
      }
      else if (res == 1)
      {
        // 模拟丢包
        continue;
      }

      int sockfd = -1;
      for (int i = 0; i < MAX_TRANSPORT_CONNECTIONS; i++)
      {
        if (tcb_list[i] != NULL && tcb_list[i]->client_portNum == seg.header.dest_port)
        {
          sockfd = i;
          break;
        }
      }
      if (sockfd < 0)
      {
        printf("seghandler: get_sockfd error\n");
        return NULL;
      }
      switch (tcb_list[sockfd]->state)
      {
      case CLOSED:
        break;
      case SYNSENT:
        if (seg.header.type == SYNACK)
        {
          tcb_list[sockfd]->state = CONNECTED;
          tcb_list[sockfd]->server_portNum = seg.header.src_port;
          printf("Clinet recv SYNACK\n");
        }
        break;
      case CONNECTED:
        if (seg.header.type == DATAACK)
        {
          pthread_mutex_lock(tcb_list[sockfd]->bufMutex);
          segBuf_t *segBuf = tcb_list[sockfd]->sendBufHead;
          while (segBuf != NULL && segBuf->seg.header.seq_num < seg.header.seq_num)
          {
            printf("seq_num: %d, recv_seq_num: %d\n", segBuf->seg.header.seq_num, seg.header.seq_num);
            segBuf_t *tmp = segBuf;
            segBuf = segBuf->next;
            free(tmp);
            tcb_list[sockfd]->sendBufHead = segBuf;
            tcb_list[sockfd]->unAck_segNum--;
          }
          pthread_mutex_unlock(tcb_list[sockfd]->bufMutex);
          printf("Client recv DATAACK\n");
        }
        break;
      case FINWAIT:
        if (seg.header.type == FINACK)
        {
          tcb_list[sockfd]->state = CLOSED;
          printf("Client recv FINACK\n");
        }
        break;
      }
    }
  }
  return 0;
}

/// @brief 发送缓冲区定时器
/// @param clienttcb 客户端套接字ID
void* sendBuf_timer(void* clienttcb)
{
  int sockfd = (int)clienttcb;
  while (1)
  {
    pthread_mutex_lock(tcb_list[sockfd]->bufMutex);
    segBuf_t *segBuf = tcb_list[sockfd]->sendBufHead;
    if (segBuf == NULL)
    {
      pthread_mutex_unlock(tcb_list[sockfd]->bufMutex);
      break;
    }
    //缓冲区非空
    pthread_mutex_unlock(tcb_list[sockfd]->bufMutex);
    unsigned int now = get_time_now();
    if (now - segBuf->sentTime > DATA_TIMEOUT / 1000000000)
    {
      pthread_mutex_lock(tcb_list[sockfd]->bufMutex);
      segBuf = tcb_list[sockfd]->sendBufHead;
      while (segBuf != NULL && segBuf != tcb_list[sockfd]->sendBufunSent)
      {
        printf("sendBuf_timer: resend seq_num: %d\n", segBuf->seg.header.seq_num);
        segBuf->sentTime = get_time_now();
        if (sip_sendseg(sip_conn, &segBuf->seg) < 0)
        {
          printf("sendBuf_timer: sip_sendseg error\n");
          return NULL;
        }
        segBuf = segBuf->next;
      }
      pthread_mutex_unlock(tcb_list[sockfd]->bufMutex);
    }
  }
  printf("sendBuf_timer: sendBuf empty\n");
  return NULL;
}

/// @brief 获取当前时间
/// @return 当前时间，进度为秒
unsigned int get_time_now()
{
  time_t now;
  time(&now);
  return (unsigned int)now;
}
