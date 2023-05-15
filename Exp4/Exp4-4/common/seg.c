
#include "seg.h"

#define SEGSTART1 0
#define SEGSTART2 1
#define SEGRECV 2
#define SEGSTOP1 3
#define SEGSTOP2 4

//STCP进程使用这个函数发送sendseg_arg_t结构(包含段及其目的节点ID)给SIP进程.
//参数sip_conn是在STCP进程和SIP进程之间连接的TCP描述符.
//如果sendseg_arg_t发送成功,就返回1,否则返回-1.
int sip_sendseg(int sip_conn, int dest_nodeID, seg_t* segPtr)
{
  //封装STCP段
  sendseg_arg_t sendseg_arg;
  sendseg_arg.nodeID = dest_nodeID;
  memcpy(&(sendseg_arg.seg), segPtr, sizeof(seg_t));
  //发送
  int res = 1;
  res = send(sip_conn, "!&", 2, 0);
  if (res <= 0)
    return -1;
  res = send(sip_conn, &sendseg_arg, sizeof(sendseg_arg_t), 0);
  if (res <= 0)
    return -1;
  res = send(sip_conn, "!#", 2, 0);
  return res > 0 ? 1 : -1;
}

//STCP进程使用这个函数来接收来自SIP进程的包含段及其源节点ID的sendseg_arg_t结构.
//参数sip_conn是STCP进程和SIP进程之间连接的TCP描述符.
//当接收到段时, 使用seglost()来判断该段是否应被丢弃并检查校验和.
//如果成功接收到sendseg_arg_t就返回1, 否则返回-1.
int sip_recvseg(int sip_conn, int* src_nodeID, seg_t* segPtr)
{
  int res = 1, check = 1;
  sendseg_arg_t sendseg_arg;
  int len = 0;
  int state = SEGSTART1;
  char c;
  while (res > 0)
  {
    switch (state)
    {
    case SEGSTART1:
      res = recv(sip_conn, &c, 1, 0);
      if (c == '!')
        state = SEGSTART2;
      break;
    case SEGSTART2:
      res = recv(sip_conn, &c, 1, 0);
      if (c == '&')
        state = SEGRECV;
      else
        state = SEGSTART1;
      break;
    case SEGRECV:
      while(len < sizeof(sendseg_arg_t))
      {
        res = recv(sip_conn, ((char*)&sendseg_arg) + len, sizeof(sendseg_arg_t) - len, 0);
        if (res <= 0) return -1;
        len += res;
      }
      state = SEGSTOP1;
      break;
    case SEGSTOP1:
      res = recv(sip_conn, &c, 1, 0);
      if (c == '!')
        state = SEGSTOP2;
      else
        return -1;
      break;
    case SEGSTOP2:
      res = recv(sip_conn, &c, 1, 0);
      if (c == '#')
      {
        *src_nodeID = sendseg_arg.nodeID;
        memcpy(segPtr, &(sendseg_arg.seg), sizeof(seg_t));
        res = seglost(segPtr);
        check = checkchecksum(segPtr);
        return res == 0 ? (check == 1 ? 0 : 1) : 1;
      }
      else
        return -1;
    }
  }
  return -1;
}

//SIP进程使用这个函数接收来自STCP进程的包含段及其目的节点ID的sendseg_arg_t结构.
//参数stcp_conn是在STCP进程和SIP进程之间连接的TCP描述符.
//如果成功接收到sendseg_arg_t就返回1, 否则返回-1.
int getsegToSend(int stcp_conn, int* dest_nodeID, seg_t* segPtr)
{
  int res = 1;
  sendseg_arg_t sendseg_arg;
  int len = 0;
  int state = SEGSTART1;
  char c;
  while (res > 0)
  {
    switch (state)
    {
    case SEGSTART1:
      res = recv(stcp_conn, &c, 1, 0);
      if (c == '!')
        state = SEGSTART2;
      break;
    case SEGSTART2:
      res = recv(stcp_conn, &c, 1, 0);
      if (c == '&')
        state = SEGRECV;
      else
        state = SEGSTART1;
      break;
    case SEGRECV:
      while (len < sizeof(sendseg_arg_t))
      {
        res = recv(stcp_conn, ((char *)&sendseg_arg) + len, sizeof(sendseg_arg_t) - len, 0);
        if (res <= 0)
          return -1;
        len += res;
      }
      state = SEGSTOP1;
      break;
    case SEGSTOP1:
      res = recv(stcp_conn, &c, 1, 0);
      if (c == '!')
        state = SEGSTOP2;
      else
        return -1;
      break;
    case SEGSTOP2:
      res = recv(stcp_conn, &c, 1, 0);
      if (c == '#')
      {
        *dest_nodeID = sendseg_arg.nodeID;
        memcpy(segPtr, &(sendseg_arg.seg), sizeof(seg_t));
        return 1;
      }
      else
        return -1;
    }
  }
  return -1;
}

//SIP进程使用这个函数发送包含段及其源节点ID的sendseg_arg_t结构给STCP进程.
//参数stcp_conn是STCP进程和SIP进程之间连接的TCP描述符.
//如果sendseg_arg_t被成功发送就返回1, 否则返回-1.
int forwardsegToSTCP(int stcp_conn, int src_nodeID, seg_t* segPtr)
{
  // 封装STCP段
  sendseg_arg_t sendseg_arg;
  sendseg_arg.nodeID = src_nodeID;
  memcpy(&(sendseg_arg.seg), segPtr, sizeof(seg_t));
  // 发送
  int res = 1;
  res = send(stcp_conn, "!&", 2, 0);
  if (res <= 0)
    return -1;
  res = send(stcp_conn, &sendseg_arg, sizeof(sendseg_arg_t), 0);
  if (res <= 0)
    return -1;
  res = send(stcp_conn, "!#", 2, 0);
  return res > 0 ? 1 : -1;
}

// 一个段有PKT_LOST_RATE/2的可能性丢失, 或PKT_LOST_RATE/2的可能性有着错误的校验和.
// 如果数据包丢失了, 就返回1, 否则返回0. 
// 即使段没有丢失, 它也有PKT_LOST_RATE/2的可能性有着错误的校验和.
// 我们在段中反转一个随机比特来创建错误的校验和.
int seglost(seg_t* segPtr)
{
  int random = rand() % 100;
  if (random < PKT_LOSS_RATE * 100)
  {
    // 50%可能性丢失段
    if (rand() % 2 == 0)
    {
      printf("seg lost\n");
      return 1;
    }
    // 50%可能性是错误的校验和
    else
    {
      // 获取数据长度
      int len = sizeof(stcp_hdr_t) + segPtr->header.length;
      // 获取要反转的随机位
      int errorbit = rand() % (len * 8);
      // 反转该比特
      char *temp = (char *)segPtr;
      temp = temp + errorbit / 8;
      *temp = *temp ^ (1 << (errorbit % 8));
      return 0;
    }
  }
  return 0;
}

//这个函数计算指定段的校验和.
//校验和计算覆盖段首部和段数据. 你应该首先将段首部中的校验和字段清零, 
//如果数据长度为奇数, 添加一个全零的字节来计算校验和.
//校验和计算使用1的补码.
unsigned short checksum(seg_t* segment)
{
  int len = sizeof(stcp_hdr_t) + segment->header.length;
  unsigned short *temp = (unsigned short *)segment;
  unsigned int sum = 0;

  segment->header.checksum = 0;
  if (len % 2 != 0)
  {
    segment->data[segment->header.length] = 0;
    len++;
  }
  for (int i = 0; i < len / 2; i++)
  {
    sum += *temp;
    temp++;
  }
  sum = (sum >> 16) + (sum & 0xffff);
  sum += (sum >> 16);
  return ~sum;
}

//这个函数检查段中的校验和, 正确时返回1, 错误时返回-1.
int checkchecksum(seg_t* segment)
{
  int len = sizeof(stcp_hdr_t) + segment->header.length;
  unsigned short *temp = (unsigned short *)segment;
  unsigned int sum = 0;

  if (len % 2 != 0)
  {
    segment->data[segment->header.length] = 0;
    len++;
  }
  for (int i = 0; i < len / 2; i++)
  {
    sum += *temp;
    temp++;
  }
  sum = (sum >> 16) + (sum & 0xffff);
  sum += (sum >> 16);
  return sum == 0xffff ? 1 : -1;
}
