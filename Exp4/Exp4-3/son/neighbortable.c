//文件名: son/neighbortable.c
//
//描述: 这个文件实现用于邻居表的API

#include "neighbortable.h"

//这个函数首先动态创建一个邻居表. 然后解析文件topology/topology.dat, 填充所有条目中的nodeID和nodeIP字段, 将conn字段初始化为-1.
//返回创建的邻居表.
nbr_entry_t* nt_create()
{
  int nbrNum = 0;
  int* nbrID;
  in_addr_t* nbrIP;
  topology_getNbrArray(nbrID, nbrIP, &nbrNum);

  nbr_entry_t* nt = (nbr_entry_t*)malloc(sizeof(nbr_entry_t) * nbrNum);
  for (int i = 0; i < nbrNum; i++)
  {
    nt[i].nodeID = nbrID[i];
    nt[i].nodeIP = nbrIP[i];
    nt[i].conn = -1;
    struct in_addr addr;
    addr.s_addr = nbrIP[i];
    printf("nodeID:%d\tnodeIP:%s\n", nt[i].nodeID, inet_ntoa(addr));
  }
  free(nbrID);
  free(nbrIP);
  exit(0);
  return nt;
}

//这个函数删除一个邻居表. 它关闭所有连接, 释放所有动态分配的内存.
void nt_destroy(nbr_entry_t* nt)
{
  return;
}

//这个函数为邻居表中指定的邻居节点条目分配一个TCP连接. 如果分配成功, 返回1, 否则返回-1.
int nt_addconn(nbr_entry_t* nt, int nodeID, int conn)
{
  return 0;
}