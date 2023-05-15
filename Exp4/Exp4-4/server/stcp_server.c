//�ļ���: server/stcp_server.c
//����: ����ļ�����STCP�������ӿ�ʵ��. 

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <sys/time.h>
#include <strings.h>
#include <unistd.h>
#include <time.h>
#include <pthread.h>
#include <assert.h>
#include "stcp_server.h"
#include "../topology/topology.h"
#include "../common/constants.h"

int sip_conn;										                      // �ص������TCP�׽���������
server_tcb_t *tcb_list[MAX_TRANSPORT_CONNECTIONS];	  // ��������TCB��
pthread_mutex_t st_mutex = PTHREAD_MUTEX_INITIALIZER; // TCB����ʻ�����
pthread_cond_t st_cond = PTHREAD_COND_INITIALIZER;	  // TCB�������������

/// @brief ��ʼ��������TCB��
/// @param conn �ص�����TCP�׽���������
void stcp_server_init(int conn)
{
  for (int i = 0; i < MAX_TRANSPORT_CONNECTIONS; i++)
  	tcb_list[i] = NULL;
  sip_conn = conn;
  pthread_t thread_seg, thread_timer;
  pthread_create(&thread_seg, NULL, seghandler, NULL);
  pthread_create(&thread_timer, NULL, timer_thread, NULL);
}

/// @brief ����STCP�������׽���
/// @param server_port �������˿ں�
/// @retval >=0 �����ɹ������ط�����TCB���е���Ŀ����
/// @retval -1 ����ʧ��
int stcp_server_sock(unsigned int server_port)
{
  for (int i = 0; i < MAX_TRANSPORT_CONNECTIONS; i++)
  {
    if (tcb_list[i] == NULL)
    {
      tcb_list[i] = (server_tcb_t *)malloc(sizeof(server_tcb_t));
      tcb_list[i]->server_nodeID = topology_getMyNodeID();
      tcb_list[i]->server_portNum = server_port;
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

/// @brief ��������STCP�ͻ��˵�����
/// @param sockfd STCP�׽���������
/// @return 1 ���ӳɹ�
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

/// @brief ��STCP�������׽��ֽ�������
/// @param sockfd STCP�׽���������
/// @param buf �������ݻ�����
/// @param length �������ݳ���
/// @retval 1 ���ճɹ�
/// @retval -1 ����ʧ��
int stcp_server_recv(int sockfd, void *buf, unsigned int length)
{
  struct timeval rtv, tv;
  tv.tv_sec = RECVBUF_POLLING_INTERVAL;
  tv.tv_usec = 0;
  while (1)
  {
    pthread_mutex_lock(tcb_list[sockfd]->bufMutex);
    if (tcb_list[sockfd]->usedBufLen >= length)
    {
      memcpy(buf, tcb_list[sockfd]->recvBuf, length);
      tcb_list[sockfd]->usedBufLen -= length;
      memmove(tcb_list[sockfd]->recvBuf, tcb_list[sockfd]->recvBuf + length, tcb_list[sockfd]->usedBufLen);
      pthread_mutex_unlock(tcb_list[sockfd]->bufMutex);
      return 1;
    }
    pthread_mutex_unlock(tcb_list[sockfd]->bufMutex);
    rtv = tv;
    select(0, NULL, NULL, NULL, &rtv);
  }
  return -1;
}

/// @brief �ر�STCP�������׽���
/// @param sockfd STCP�׽���������
/// @retval 1 �رճɹ�
/// @retval -1 �ر�ʧ��
int stcp_server_close(int sockfd)
{
  if (tcb_list[sockfd]->state == CLOSED)
  {
    int cli_nodeID = tcb_list[sockfd]->client_nodeID;
    pthread_mutex_lock(&st_mutex);
    free(tcb_list[sockfd]->bufMutex);
    free(tcb_list[sockfd]->recvBuf);
    free(tcb_list[sockfd]);
    tcb_list[sockfd] = NULL;
    pthread_mutex_unlock(&st_mutex);
    printf("Server connect close, client ID:%d\n", cli_nodeID);
    return 1;
  }
  return -1;
}

/// @brief ������յ���STCP��
void *seghandler(void *arg)
{
  while (1)
  {
    seg_t seg;
    int srcID;
    int ret;
    ret = sip_recvseg(sip_conn, &srcID, &seg);
    if (ret == -1)
    {
      // �ص��������ӹرջ����
      printf("Server sip connect closed\n");
      exit(0);
    }
    else if (ret == 1)
    {
      // ���Ķ�ʧ
      continue;
    }
    // ���ճɹ�
    int sockfd = -1;
    for (int i = 0; i < MAX_TRANSPORT_CONNECTIONS; i++)
    {
      if (tcb_list[i] != NULL 
      && tcb_list[i]->server_portNum == seg.header.dest_port)
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
        printf("Server recv SYN from %d\n", srcID);
        tcb_list[sockfd]->client_nodeID = srcID;
        tcb_list[sockfd]->client_portNum = seg.header.src_port;
        tcb_list[sockfd]->expect_seqNum = seg.header.seq_num;
        tcb_list[sockfd]->state = CONNECTED;
        pthread_cond_signal(&st_cond);
        seg.header.src_port = tcb_list[sockfd]->server_portNum;
        seg.header.dest_port = tcb_list[sockfd]->client_portNum;
        seg.header.type = SYNACK;
        seg.header.seq_num = tcb_list[sockfd]->expect_seqNum;
        seg.header.checksum = checksum(&seg);
        sip_sendseg(sip_conn, srcID, &seg);
        printf("Server send SYNACK to %d\n", srcID);
      }
      break;
    case CONNECTED:
      if (seg.header.type == FIN)
      {
        printf("Server recv FIN from %d\n", srcID);
        tcb_list[sockfd]->state = CLOSEWAIT;
        seg.header.src_port = tcb_list[sockfd]->server_portNum;
        seg.header.dest_port = tcb_list[sockfd]->client_portNum;
        seg.header.type = FINACK;
        seg.header.seq_num = tcb_list[sockfd]->expect_seqNum;
        seg.header.checksum = checksum(&seg);
        sip_sendseg(sip_conn, srcID, &seg);
        printf("Server send FINACK to %d\n", srcID);
      }
      else if (seg.header.type == SYN)
      {
        printf("Server recv SYN when CONNECTED\n");
        tcb_list[sockfd]->expect_seqNum = seg.header.seq_num;
        seg.header.src_port = tcb_list[sockfd]->server_portNum;
        seg.header.dest_port = tcb_list[sockfd]->client_portNum;
        seg.header.type = SYNACK;
        seg.header.seq_num = tcb_list[sockfd]->expect_seqNum;
        seg.header.checksum = checksum(&seg);
        sip_sendseg(sip_conn, srcID, &seg);
        printf("Server send SYNACK\n");
      }
      else if (seg.header.type == DATA)
      {
        // ���յ����ݶ�
        printf("seq_num = %d, expect_seqNum = %d\n", seg.header.seq_num, tcb_list[sockfd]->expect_seqNum);
        if (seg.header.seq_num == tcb_list[sockfd]->expect_seqNum)
        {
          printf("Server recv DATA from %d\n", srcID);
          pthread_mutex_lock(tcb_list[sockfd]->bufMutex);
          // ���յ������ݳ��ȳ�����������С
          if (tcb_list[sockfd]->usedBufLen + seg.header.length > RECEIVE_BUF_SIZE)
          {
            printf("Server recv DATA, but recvBuf full\n");
            pthread_mutex_unlock(tcb_list[sockfd]->bufMutex);
            break;
          }
          memcpy(tcb_list[sockfd]->recvBuf + tcb_list[sockfd]->usedBufLen, seg.data, seg.header.length);
          pthread_mutex_unlock(tcb_list[sockfd]->bufMutex);
          tcb_list[sockfd]->usedBufLen += seg.header.length;
          tcb_list[sockfd]->expect_seqNum += seg.header.length;
        }
        else
        {
          printf("Server recv DATA from %d, but seq_num wrong\n", srcID);
        }
        seg.header.src_port = tcb_list[sockfd]->server_portNum;
        seg.header.dest_port = tcb_list[sockfd]->client_portNum;
        seg.header.seq_num = tcb_list[sockfd]->expect_seqNum;
        seg.header.type = DATAACK;
        seg.header.checksum = checksum(&seg);
        sip_sendseg(sip_conn, srcID, &seg);
        printf("Server send DATAACK\n");
      }
      break;
    case CLOSEWAIT:
      if (seg.header.type == FIN)
      {
        printf("Server recv FIN from %d\n", srcID);
        seg.header.src_port = tcb_list[sockfd]->server_portNum;
        seg.header.dest_port = tcb_list[sockfd]->client_portNum;
        seg.header.type = FINACK;
        seg.header.seq_num = tcb_list[sockfd]->expect_seqNum;
        seg.header.checksum = checksum(&seg);
        sip_sendseg(sip_conn, srcID, &seg);
        printf("Server send FINACK\n");
      }
      break;
    }
    pthread_mutex_unlock(&st_mutex);
  }
  return NULL;
}

/// @brief ��ʱ���̣߳�����CLOSEWAIT״̬�ĳ�ʱ
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
      // CLOSEWAIT��ʱ
      pthread_mutex_lock(&st_mutex);
      for (int i = 0; i < MAX_TRANSPORT_CONNECTIONS; i++)
      {
        if (tcb_list[i] != NULL && tcb_list[i]->state == CLOSEWAIT)
        {
          tcb_list[i]->state = CLOSED;
          tcb_list[i]->usedBufLen = 0;
          // printf("Server sockfd %d closewait timeout\n", i);
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