// �ļ��� pkt.c

#include "pkt.h"

#define PKTSTART1 0
#define PKTSTART2 1
#define PKTRECV 2
#define PKTSTOP1 3
#define PKTSTOP2 4

// son_sendpkt()��SIP���̵���, ��������Ҫ��SON���̽����ķ��͵��ص�������. SON���̺�SIP����ͨ��һ������TCP���ӻ���.
// ��son_sendpkt()��, ���ļ�����һ���Ľڵ�ID����װ�����ݽṹsendpkt_arg_t, ��ͨ��TCP���ӷ��͸�SON����. 
// ����son_conn��SIP���̺�SON����֮���TCP�����׽���������.
// ��ͨ��SIP���̺�SON����֮���TCP���ӷ������ݽṹsendpkt_arg_tʱ, ʹ��'!&'��'!#'��Ϊ�ָ���, ����'!& sendpkt_arg_t�ṹ !#'��˳����.
// ������ͳɹ�, ����1, ���򷵻�-1.
int son_sendpkt(int nextNodeID, sip_pkt_t* pkt, int son_conn)
{
  int res = 1;
  sendpkt_arg_t sendpkt_arg;
  sendpkt_arg.nextNodeID = nextNodeID;
  memcpy(&sendpkt_arg.pkt, pkt, sizeof(sip_pkt_t));
  res = send(son_conn, "!&", 2, 0);
  if (res <= 0)
    return -1;
  res = send(son_conn, &sendpkt_arg, sizeof(sendpkt_arg_t), 0);
  if (res <= 0)
    return -1;
  res = send(son_conn, "!#", 2, 0);
  return res > 0 ? 1 : -1;
}

// son_recvpkt()������SIP���̵���, �������ǽ�������SON���̵ı���. 
// ����son_conn��SIP���̺�SON����֮��TCP���ӵ��׽���������. ����ͨ��SIP���̺�SON����֮���TCP���ӷ���, ʹ�÷ָ���!&��!#. 
// Ϊ�˽��ձ���, �������ʹ��һ���򵥵�����״̬��FSM
// PKTSTART1 -- ��� 
// PKTSTART2 -- ���յ�'!', �ڴ�'&' 
// PKTRECV -- ���յ�'&', ��ʼ��������
// PKTSTOP1 -- ���յ�'!', �ڴ�'#'�Խ������ݵĽ��� 
// ����ɹ����ձ���, ����1, ���򷵻�-1.
int son_recvpkt(sip_pkt_t* pkt, int son_conn)
{
  int res = 1;
  size_t len = 0;
  int state = PKTSTART1;
  char c;
  while (res > 0)
  {
    switch (state)
    {
    case PKTSTART1:
      res = recv(son_conn, &c, 1, 0);
      if (c == '!')
        state = PKTSTART2;
      break;
    case PKTSTART2:
      res = recv(son_conn, &c, 1, 0);
      if (c == '&')
        state = PKTRECV;
      else
        state = PKTSTART1;
      break;
    case PKTRECV:
      while (len < sizeof(sip_pkt_t))
      {
        res = recv(son_conn, ((char *)pkt) + len, sizeof(sip_pkt_t) - len, 0);
        if (res <= 0)
          return -1;
        len += (size_t)res;
      }
      state = PKTSTOP1;
      break;
    case PKTSTOP1:
      res = recv(son_conn, &c, 1, 0);
      if (c == '!')
        state = PKTSTOP2;
      else
        return -1;
      break;
    case PKTSTOP2:
      res = recv(son_conn, &c, 1, 0);
      if (c == '#')
        return 1;
      else
        return -1;
      break;
    }
  }
  return -1;
}

// ���������SON���̵���, �������ǽ������ݽṹsendpkt_arg_t.
// ���ĺ���һ���Ľڵ�ID����װ��sendpkt_arg_t�ṹ.
// ����sip_conn����SIP���̺�SON����֮���TCP���ӵ��׽���������. 
// sendpkt_arg_t�ṹͨ��SIP���̺�SON����֮���TCP���ӷ���, ʹ�÷ָ���!&��!#. 
// Ϊ�˽��ձ���, �������ʹ��һ���򵥵�����״̬��FSM
// PKTSTART1 -- ��� 
// PKTSTART2 -- ���յ�'!', �ڴ�'&' 
// PKTRECV -- ���յ�'&', ��ʼ��������
// PKTSTOP1 -- ���յ�'!', �ڴ�'#'�Խ������ݵĽ���
// ����ɹ�����sendpkt_arg_t�ṹ, ����1, ���򷵻�-1.
int getpktToSend(sip_pkt_t* pkt, int* nextNode,int sip_conn)
{
  int res = 1;
  size_t len = 0;
  int state = PKTSTART1;
  sendpkt_arg_t sendpkt_arg;
  char c;
  while (res > 0)
  {
    switch (state)
    {
    case PKTSTART1:
      res = recv(sip_conn, &c, 1, 0);
      if (c == '!')
        state = PKTSTART2;
      break;
    case PKTSTART2:
      res = recv(sip_conn, &c, 1, 0);
      if (c == '&')
        state = PKTRECV;
      else
        state = PKTSTART1;
      break;
    case PKTRECV:
      while (len < sizeof(sendpkt_arg_t))
      {
        res = recv(sip_conn, ((char *)&sendpkt_arg) + len, sizeof(sendpkt_arg_t) - len, 0);
        if (res <= 0)
          return -1;
        len += (size_t)res;
      }
      state = PKTSTOP1;
      break;
    case PKTSTOP1:
      res = recv(sip_conn, &c, 1, 0);
      if (c == '!')
        state = PKTSTOP2;
      else
        return -1;
      break;
    case PKTSTOP2:
      res = recv(sip_conn, &c, 1, 0);
      if (c == '#')
      {
        memcpy(pkt, &sendpkt_arg.pkt, sizeof(sip_pkt_t));
        *nextNode = sendpkt_arg.nextNodeID;
        return 1;
      }
      else
        return -1;
      break;
    }
  }
  return -1;
}

// forwardpktToSIP()��������SON���̽��յ������ص����������ھӵı��ĺ󱻵��õ�. 
// SON���̵����������������ת����SIP����. 
// ����sip_conn��SIP���̺�SON����֮���TCP���ӵ��׽���������. 
// ����ͨ��SIP���̺�SON����֮���TCP���ӷ���, ʹ�÷ָ���!&��!#, ����'!& ���� !#'��˳����. 
// ������ķ��ͳɹ�, ����1, ���򷵻�-1.
int forwardpktToSIP(sip_pkt_t* pkt, int sip_conn)
{
  int res = 1;
  res = send(sip_conn, "!&", 2, 0);
  if (res <= 0)
    return -1;
  res = send(sip_conn, pkt, sizeof(sip_pkt_t), 0);
  if (res <= 0)
    return -1;
  res = send(sip_conn, "!#", 2, 0);
  return res > 0 ? 1 : -1;
}

// sendpkt()������SON���̵���, �������ǽ�������SIP���̵ı��ķ��͸���һ��.
// ����conn�ǵ���һ���ڵ��TCP���ӵ��׽���������.
// ����ͨ��SON���̺����ھӽڵ�֮���TCP���ӷ���, ʹ�÷ָ���!&��!#, ����'!& ���� !#'��˳����. 
// ������ķ��ͳɹ�, ����1, ���򷵻�-1.
int sendpkt(sip_pkt_t* pkt, int conn)
{
  int res = 1;
  res = send(conn, "!&", 2, 0);
  if (res <= 0)
    return -1;
  res = send(conn, pkt, sizeof(sip_pkt_t), 0);
  if (res <= 0)
    return -1;
  res = send(conn, "!#", 2, 0);
  return res > 0 ? 1 : -1;
}

// recvpkt()������SON���̵���, �������ǽ��������ص����������ھӵı���.
// ����conn�ǵ����ھӵ�TCP���ӵ��׽���������.
// ����ͨ��SON���̺����ھ�֮���TCP���ӷ���, ʹ�÷ָ���!&��!#. 
// Ϊ�˽��ձ���, �������ʹ��һ���򵥵�����״̬��FSM
// PKTSTART1 -- ��� 
// PKTSTART2 -- ���յ�'!', �ڴ�'&' 
// PKTRECV -- ���յ�'&', ��ʼ��������
// PKTSTOP1 -- ���յ�'!', �ڴ�'#'�Խ������ݵĽ��� 
// ����ɹ����ձ���, ����1, ���򷵻�-1.
int recvpkt(sip_pkt_t* pkt, int conn)
{
  int res = 1;
  size_t len = 0;
  char buf[1600];
  int state = PKTSTART1;
  char c;
  while (res > 0)
  {
    switch (state)
    {
    case PKTSTART1:
      res = recv(conn, &c, 1, 0);
      if (c == '!')
        state = PKTSTART2;
      break;
    case PKTSTART2:
      res = recv(conn, &c, 1, 0);
      if (c == '&')
        state = PKTRECV;
      else
        state = PKTSTART1;
      break;
    case PKTRECV:
      while (len < sizeof(sip_pkt_t))
      {
        res = recv(conn, buf + len, sizeof(sip_pkt_t) - len, 0);
        if (res <= 0)
          return -1;
        len += (size_t)res;
      }
      memcpy(pkt, buf, sizeof(sip_pkt_t));
      state = PKTSTOP1;
      break;
    case PKTSTOP1:
      res = recv(conn, &c, 1, 0);
      if (c == '!')
        state = PKTSTOP2;
      else
        return -1;
      break;
    case PKTSTOP2:
      res = recv(conn, &c, 1, 0);
      if (c == '#')
        return 1;
      else
        return -1;
      break;
    }
  }
  return -1;
}
