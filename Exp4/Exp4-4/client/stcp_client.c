//�ļ���: client/stcp_client.c
//
//����: ����ļ�����STCP�ͻ��˽ӿ�ʵ�� 

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <sys/time.h>
#include <assert.h>
#include <strings.h>
#include <unistd.h>
#include <sys/time.h>
#include <time.h>
#include <pthread.h>
#include "../topology/topology.h"
#include "stcp_client.h"
#include "../common/seg.h"

int sip_conn;										  // �ص������TCP�׽���������
client_tcb_t *tcb_list[MAX_TRANSPORT_CONNECTIONS];	  // �ͻ���TCB��
pthread_mutex_t st_mutex = PTHREAD_MUTEX_INITIALIZER; // TCB��������
pthread_cond_t st_cond = PTHREAD_COND_INITIALIZER;	  // TCB����������

/// @brief ��ʼ���ͻ���TCB��
/// @param conn �ص������TCP�׽���������
void stcp_client_init(int conn)
{
  for (int i = 0; i < MAX_TRANSPORT_CONNECTIONS; i++)
  	tcb_list[i] = NULL;
  sip_conn = conn;
  pthread_t thread;
  pthread_create(&thread, NULL, seghandler, NULL);
}

/// @brief ����STCP�ͻ����׽���
/// @param client_port �ͻ��˶˿ں�
/// @retval >=0 �����ɹ�������TCB���е���Ŀ����
/// @retval -1 ����ʧ��
int stcp_client_sock(unsigned int client_port)
{
  for (int i = 0; i < MAX_TRANSPORT_CONNECTIONS; i++)
  {
    if (tcb_list[i] == NULL)
    {
      client_tcb_t *tcb = (client_tcb_t *)malloc(sizeof(client_tcb_t));
      tcb->client_nodeID = topology_getMyNodeID();
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

/// @brief ����STCP������
/// @param sockfd �׽���ID
/// @param server_port �������˿ں�
/// @retval 1 ���ӳɹ�
/// @retval -1 ����ʧ��
int stcp_client_connect(int sockfd, int nodeID, unsigned int server_port) 
{
  tcb_list[sockfd]->server_nodeID = nodeID;
  tcb_list[sockfd]->server_portNum = server_port;
  seg_t seg;
  seg.header.src_port = tcb_list[sockfd]->client_portNum;
  seg.header.dest_port = tcb_list[sockfd]->server_portNum;
  seg.header.type = SYN;
  seg.header.length = 0;
  seg.header.seq_num = tcb_list[sockfd]->next_seqNum;
  seg.header.checksum = checksum(&seg);

  if (sip_sendseg(sip_conn, nodeID, &seg) < 0)
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
    if (sip_sendseg(sip_conn, nodeID, &seg) < 0)
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

/// @brief ��STCP��������������
/// @param sockfd �׽���ID
/// @param data �������ݻ�����
/// @param length �������ݳ���
/// @retval 1 ���ͳɹ�
/// @retval -1 ����ʧ��
int stcp_client_send(int sockfd, void *data, unsigned int length)
{
  // printf("Client send data in\n");
  // printf("Client send length: %d\n", length);
  // �����ݷֶ�
  int segCnt = length / MAX_SEG_LEN + (length % MAX_SEG_LEN != 0 ? 1 : 0);
  int remainLen = length;
  for (int i = 0; i < segCnt; i++)
  {
    int segLen = remainLen > MAX_SEG_LEN ? MAX_SEG_LEN : remainLen;
    remainLen -= segLen;
    segBuf_t *segBuf = (segBuf_t *)malloc(sizeof(segBuf_t));
    segBuf->seg.header.src_port = tcb_list[sockfd]->client_portNum;
    segBuf->seg.header.dest_port = tcb_list[sockfd]->server_portNum;
    segBuf->seg.header.type = DATA;
    segBuf->seg.header.length = segLen;
    segBuf->seg.header.seq_num = tcb_list[sockfd]->next_seqNum;
    memcpy(segBuf->seg.data, data + i * MAX_SEG_LEN, segLen);
    segBuf->seg.header.checksum = checksum(&segBuf->seg);
    segBuf->sentTime = get_time_now();
    segBuf->next = NULL;
    pthread_mutex_lock(tcb_list[sockfd]->bufMutex);
    if (tcb_list[sockfd]->sendBufHead == NULL)
    {
      // ������Ϊ��
      tcb_list[sockfd]->sendBufHead = segBuf;
      tcb_list[sockfd]->sendBufunSent = segBuf;
      tcb_list[sockfd]->sendBufTail = segBuf;
      // ����sendBuf_timer�߳�
      pthread_t thread;
      pthread_create(&thread, NULL, sendBuf_timer, (void *)sockfd);
    }
    else
    {
      tcb_list[sockfd]->sendBufTail->next = segBuf;
      tcb_list[sockfd]->sendBufTail = segBuf;
      // ��������STCP����ȫ������
      if (tcb_list[sockfd]->sendBufunSent == NULL)
        tcb_list[sockfd]->sendBufunSent = segBuf;
    }
    pthread_mutex_unlock(tcb_list[sockfd]->bufMutex);
    tcb_list[sockfd]->next_seqNum += segLen;
  }
  // �ӷ��ͻ������е�һ��δ���Ͷο�ʼ���ͣ�ֱ���ѷ��͵�δ��ȷ�����ݶε���Ŀ����GBN_WINDOW
  // printf("unAck_segNum: %d\n", tcb_list[sockfd]->unAck_segNum);
  struct timeval tv, rtv;
  tv.tv_sec = 0;
  tv.tv_usec = 1000; // 1us = 1000ns
  while (1)
  {
    rtv = tv;
    select(0, NULL, NULL, NULL, &rtv);
    while (tcb_list[sockfd]->unAck_segNum < GBN_WINDOW)
    {
      pthread_mutex_lock(tcb_list[sockfd]->bufMutex);
      if (tcb_list[sockfd]->sendBufunSent == NULL)
      {
        // ��������STCP����ȫ������
        pthread_mutex_unlock(tcb_list[sockfd]->bufMutex);
        // printf("Client send data out\n");
        return 1;
      }
      tcb_list[sockfd]->sendBufunSent->sentTime = get_time_now();
      seg_t seg = tcb_list[sockfd]->sendBufunSent->seg;
      pthread_mutex_unlock(tcb_list[sockfd]->bufMutex);
      printf("send seq_num: %d\n", seg.header.seq_num);
      if (sip_sendseg(sip_conn, tcb_list[sockfd]->server_nodeID, &seg) < 0)
      {
        printf("stcp_client_send: sip_sendseg error\n");
        return -1;
      }
      printf("Client send DATA\n");
      tcb_list[sockfd]->sendBufunSent = tcb_list[sockfd]->sendBufunSent->next;
      tcb_list[sockfd]->unAck_segNum++;
    }
  }
  // printf("Client send data out\n");
  return 1;
}

/// @brief �ر�STCP����.
/// @param sockfd �׽���ID
/// @retval 1 �رճɹ�
/// @retval -1 �ر�ʧ��
int stcp_client_disconnect(int sockfd)
{
  seg_t seg;
  seg.header.src_port = tcb_list[sockfd]->client_portNum;
  seg.header.dest_port = tcb_list[sockfd]->server_portNum;
  seg.header.seq_num = tcb_list[sockfd]->next_seqNum;
  seg.header.ack_num = 0;
  seg.header.length = 0;
  seg.header.type = FIN;
  seg.header.rcv_win = 0;
  seg.header.checksum = checksum(&seg);

  if (sip_sendseg(sip_conn, tcb_list[sockfd]->server_nodeID, &seg) < 0)
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
    if (sip_sendseg(sip_conn, tcb_list[sockfd]->server_nodeID, &seg) < 0)
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

/// @brief �ر�STCP�ͻ���
/// @param sockfd �׽���ID
/// @retval 1 �رճɹ�
/// @retval -1 �ر�ʧ��
int stcp_client_close(int sockfd)
{
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

/// @brief �������յ���STCP��
void *seghandler(void *arg)
{
  fd_set rset, set;
  // struct timeval rtv, tv;
  int res;
  FD_ZERO(&set);
  FD_SET(sip_conn, &set);
  // rtv.tv_sec = 0;
  // rtv.tv_usec = SYN_TIMEOUT / 1000; // 1us = 1000ns

  while (1)
  {
    // tv = rtv;
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
      int srcID;
      int res = sip_recvseg(sip_conn, &srcID, &seg);
      if (res == -1)
      {
        printf("Client sip connect closed\n");
        return NULL;
      }
      else if (res == 1)
      {
        // ģ�ⶪ��
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
          tcb_list[sockfd]->server_nodeID = srcID;
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
            printf("assumed seq_num: %d, recv seq_num: %d\n", segBuf->seg.header.seq_num, seg.header.seq_num);
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

/// @brief ���ͻ�������ʱ��
/// @param clienttcb �ͻ����׽���ID
void *sendBuf_timer(void *clienttcb)
{
  int sockfd = (int)clienttcb;
  struct timeval tv, rtv;
  tv.tv_sec = 0;
  tv.tv_usec = SENDBUF_POLLING_INTERVAL / 10000; // 1us = 1000ns

  while (1)
  {
    rtv = tv;
    select(0, NULL, NULL, NULL, &rtv);
    // ÿ��SENDBUF_POLLING_INTERVALʱ��Ͳ�ѯ��һ���ѷ��͵�δ��ȷ�϶�
    pthread_mutex_lock(tcb_list[sockfd]->bufMutex);
    segBuf_t *segBuf = tcb_list[sockfd]->sendBufHead;
    if (segBuf == NULL)
    {
      pthread_mutex_unlock(tcb_list[sockfd]->bufMutex);
      break;
    }
    // �������ǿ�
    pthread_mutex_unlock(tcb_list[sockfd]->bufMutex);
    unsigned int now = get_time_now();
    // �����һ���ѷ��͵�δ��ȷ�϶εķ���ʱ�䳬��DATA_TIMEOUTʱ�䣬���ش������ѷ��͵�δ��ȷ�϶�
    printf("now: %d, sentTime: %d\n", now, segBuf->sentTime);
    printf("now - sentTime: %d\n", now - segBuf->sentTime);
    if (now - segBuf->sentTime > DATA_TIMEOUT / 1000000000)
    {
      pthread_mutex_lock(tcb_list[sockfd]->bufMutex);
      segBuf = tcb_list[sockfd]->sendBufHead;
      while (segBuf != NULL && segBuf != tcb_list[sockfd]->sendBufunSent)
      {
        printf("Client resend data seq_num: %d\n", segBuf->seg.header.seq_num);
        segBuf->sentTime = get_time_now();
        if (sip_sendseg(sip_conn, tcb_list[sockfd]->server_nodeID, &segBuf->seg) < 0)
        {
          printf("sendBuf_timer: sip_sendseg error\n");
          return NULL;
        }
        segBuf = segBuf->next;
      }
      pthread_mutex_unlock(tcb_list[sockfd]->bufMutex);
    }
  }
  // printf("Client sendBuf empty\n");
  return NULL;
}

/// @brief ��ȡ��ǰʱ��
/// @return ��ǰʱ�䣬����Ϊ��
unsigned int get_time_now()
{
  time_t now;
  time(&now);
  return (unsigned int)now;
}