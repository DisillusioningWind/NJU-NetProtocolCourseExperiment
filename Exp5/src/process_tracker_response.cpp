
#include "btdata.h"
#include "util.h"

// 读取并处理来自Tracker的HTTP响应, 确认它格式正确, 然后从中提取数据. 
// 一个Tracker的HTTP响应格式如下所示:
// HTTP/1.0 200 OK       (17个字节,包括最后的\r\n)
// Content-Length: X     (到第一个空格为16个字节) 注意: X是一个数字
// Content-Type: text/plain (26个字节)
// Pragma: no-cache (18个字节)
// \r\n  (空行, 表示数据的开始)
// data                  注意: 从这开始是数据, 但并没有一个data标签
tracker_response* preprocess_tracker_response(int sockfd)
{ 
   char rcvline[MAXLINE];
   char tmp[MAXLINE];
   char* data;
   int len;
   int offset = 0;
   int datasize = -1;
   printf("Reading tracker response...\n");
   // HTTP LINE
   len = recv(sockfd,rcvline,17,0);
   if(len < 0)
   {
     perror("Error, cannot read socket from tracker");
     exit(-6);
   }
   strncpy(tmp,rcvline,17);
   
   if(strncmp(tmp,"HTTP/1.1 200 OK\r\n",strlen("HTTP/1.1 200 OK\r\n")))
   {
     perror("Error, didn't match HTTP line");
     exit(-6);
   }
   memset(rcvline,0xFF,MAXLINE);
   memset(tmp,0x0,MAXLINE);
   // Content-Type
   len = recv(sockfd,rcvline,26,0);
   if(len <= 0)
   {
     perror("Error, cannot read socket from tracker");
     exit(-6);
   }
   strncpy(tmp,rcvline,26);
   if(strncmp(tmp,"Content-Type: text/plain\r\n",strlen("Content-Type: text/plain\r\n")))
   {
     perror("Error, didn't match Content-Type line");
     exit(-6);
   }
   memset(rcvline,0xFF,MAXLINE);
   memset(tmp,0x0,MAXLINE);
   // Content-Length
   len = recv(sockfd,rcvline,15,0);
   if(len <= 0)
   {
     perror("Error, cannot read socket from tracker");
     exit(-6);
   }
   strncpy(tmp,rcvline,15);
   if(strncmp(tmp,"Content-Length:",strlen("Content-Length:")))
   {
     printf("tmp   :%s|len:%ld\n",tmp, strlen(tmp));
     printf("should:%s|len:%ld\n", "Content-Length:", strlen("Content-Length:"));
     perror("Error, didn't match Content-Length line");
     exit(-6);
   }
   memset(rcvline,0xFF,MAXLINE);
   memset(tmp,0x0,MAXLINE);
   // 读取Content-Length的数据部分
   char c[2];
   char num[MAXLINE];
   int count = 0;
   c[0] = 0; c[1] = 0;
   while(c[0] != '\r' && c[1] != '\n')
   {
      len = recv(sockfd,rcvline,1,0);
      if(len <= 0)
      {
        perror("Error, cannot read socket from tracker");
        exit(-6);
      }
      num[count] = rcvline[0];
      c[0] = c[1];
      c[1] = num[count];
      count++;
   }
   datasize = atoi(num);
   //printf("NUMBER RECEIVED: %d\n",datasize);
   memset(rcvline,0xFF,MAXLINE);
   memset(num,0x0,MAXLINE);
   // 去除响应中额外的\r\n空行
   len = recv(sockfd,rcvline,2,0);
   if(len <= 0)
   {
     perror("Error, cannot read socket from tracker");
     exit(-6);
   }

   // 分配空间并读取数据, 为结尾的\0预留空间
   int i; 
   data = (char*)malloc((datasize+1)*sizeof(char));
   for(i=0; i<datasize; i++)
   {
      len = recv(sockfd,data+i,1,0);
      if(len < 0)
      {
        perror("Error, cannot read socket from tracker");
        exit(-6);
      }
   }
   data[datasize] = '\0';
   
   for(i=0; i<datasize; i++)
     printf("%c",data[i]);
   printf("\n");
   
   // 分配, 填充并返回tracker_response结构.
   tracker_response* ret;
   ret = (tracker_response*)malloc(sizeof(tracker_response));
   if(ret == NULL)
   {
     printf("Error allocating tracker_response ptr\n");
     return 0;
   }
   ret->size = datasize;
   ret->data = data;

   return ret;
}

// 解码B编码的数据, 将解码后的数据放入tracker_data结构
tracker_data* get_tracker_data(char* data, int len)
{
  tracker_data* ret;
  be_node* ben_res;
  ben_res = be_decoden(data,len);

  if(ben_res->type != BE_DICT)
  {
    perror("Data not of type dict");
    exit(-12);
  }
 
  ret = (tracker_data*)malloc(sizeof(tracker_data));
  if(ret == NULL)
  {
    perror("Could not allcoate tracker_data");
    exit(-12);
  }

  // 遍历键并测试它们
  int i;
  for (i=0; i < ben_res->len; i++)
  {
    // printf("%s\n",ben_res->val.d[i].key);
    // 检查是否有失败键
    if(!strncmp(ben_res->val.d[i].key,"failure reason",strlen("failure reason")))
    {
      printf("Error: %s",ben_res->val.d[i].val->val.s);
      exit(-12);
    }
    // interval键
    if(!strncmp(ben_res->val.d[i].key,"interval",strlen("interval")))
    {
      ret->interval = (int)ben_res->val.d[i].val->val.i;
    }
    // peers键
    if(!strncmp(ben_res->val.d[i].key,"peers",strlen("peers")))
    { 
      // printf("node type:%d\n", ben_res->val.d[i].val->type);
      be_node *peer_list = ben_res->val.d[i].val;
      get_peers(ret,peer_list);
    }
  }

  be_free(ben_res);

  return ret;
}
// 处理来自Tracker的二进制模式的peer列表
void get_peers(tracker_data* td, be_node* peer_list)
{
  int i;
  int numpeers = peer_list->len / 6;

  // printf("peer len: %d\n",peer_list->len);

  // 为peer分配空间
  td->numpeers = numpeers;
  td->peers = (peerdata*)malloc(numpeers*sizeof(peerdata));
  if(td->peers == NULL)
  {
    perror("Couldn't allocate peers");
    exit(-12);
  }

  // 获取每个peer的数据
  for (i=0; i < numpeers; i++)
  {
    // 格式化
    char ip[4], ipstr[16], port[2], portstr[6];
    strncpy(ip, peer_list->val.s + (i * 6), 4);
    strncpy(port, peer_list->val.s + (i * 6) + 4, 2);
    sprintf(ipstr, "%d.%d.%d.%d", ip[0] & 0xff, ip[1] & 0xff, ip[2] & 0xff, ip[3] & 0xff);
    sprintf(portstr, "%d", (port[0] & 0xff) << 8 | (port[1] & 0xff));
    // 填充ip和端口
    td->peers[i].ip = (char *)malloc(16 * sizeof(char));
    td->peers[i].port = atoi(portstr);
    strcpy(td->peers[i].ip, ipstr);
    // 打印peer数据
    // printf("Peer %d: %s:%d\n",i,td->peers[i].ip,td->peers[i].port);
  }
  return;
}

// 给出一个peerdata的指针和一个peer的字典数据, 填充peerdata结构
void get_peer_data(peerdata* peer, be_node* ben_res)
{
  int i;
  
  if(ben_res->type != BE_DICT)
  {
    perror("Don't have a dict for this peer");
    exit(-12);
  }

  // 遍历键并填充peerdata结构
  for (i=0; ben_res->val.d[i].val != NULL; i++)
  { 
    //printf("%s\n",ben_res->val.d[i].key);
    
    // peer id键
    if(!strncmp(ben_res->val.d[i].key,"peer id",strlen("peer id")))
    {
      //printf("Peer id: %s\n", ben_res->val.d[i].val->val.s);
      memcpy(peer->id,ben_res->val.d[i].val->val.s,20);
      peer->id[20] = '\0';
      /*
      int idl;
      printf("Peer id: ");
      for(idl=0; idl<len; idl++)
        printf("%02X ",(unsigned char)peer->id[idl]);
      printf("\n");
      */
    }
    // ip键
    if(!strncmp(ben_res->val.d[i].key,"ip",strlen("ip")))
    {
      int len;
      //printf("Peer ip: %s\n",ben_res->val.d[i].val->val.s);
      len = strlen(ben_res->val.d[i].val->val.s);
      peer->ip = (char*)malloc((len+1)*sizeof(char));
      strcpy(peer->ip,ben_res->val.d[i].val->val.s);
    }
    // port键
    if(!strncmp(ben_res->val.d[i].key,"port",strlen("port")))
    {
      //printf("Peer port: %d\n",ben_res->val.d[i].val->val.i);
      peer->port = ben_res->val.d[i].val->val.i;
    }
  }
}
