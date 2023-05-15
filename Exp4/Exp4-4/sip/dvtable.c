
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "../common/constants.h"
#include "../topology/topology.h"
#include "dvtable.h"

//这个函数动态创建距离矢量表.
//距离矢量表包含n+1个条目, 其中n是这个节点的邻居数,剩下1个是这个节点本身.
//距离矢量表中的每个条目是一个dv_t结构,它包含一个源节点ID和一个有N个dv_entry_t结构的数组, 其中N是重叠网络中节点总数.
//每个dv_entry_t包含一个目的节点地址和从该源节点到该目的节点的链路代价.
//距离矢量表也在这个函数中初始化.从这个节点到其邻居的链路代价使用提取自topology.dat文件中的直接链路代价初始化.
//其他链路代价被初始化为INFINITE_COST.
//该函数返回动态创建的距离矢量表.
dv_t* dvtable_create()
{
  node_list_t* node_list = topology_getNodeArray();
  node_list_t* nbr_list = topology_getNbrArray();
  int nodeNum = node_list->nodeNum;
  int nbrNum = nbr_list->nodeNum;
  dv_t* dvtable = (dv_t*)malloc(sizeof(dv_t) * (nbrNum + 1));
  for(int i = 0; i < nbrNum; i++)
  {
    dvtable[i].nodeID = nbr_list->IDArr[i];
    dvtable[i].dvEntry = (dv_entry_t*)malloc(sizeof(dv_entry_t) * nodeNum);
    for(int j = 0; j < nodeNum; j++)
    {
      dvtable[i].dvEntry[j].nodeID = node_list->IDArr[j];
      if(dvtable[i].nodeID == dvtable[i].dvEntry[j].nodeID)
        dvtable[i].dvEntry[j].cost = 0;
      else
        dvtable[i].dvEntry[j].cost = INFINITE_COST;
    }
  }
  dvtable[nbrNum].nodeID = topology_getMyNodeID();
  dvtable[nbrNum].dvEntry = (dv_entry_t*)malloc(sizeof(dv_entry_t) * nodeNum);
  for(int i = 0; i < nodeNum; i++)
  {
    dvtable[nbrNum].dvEntry[i].nodeID = node_list->IDArr[i];
    if(dvtable[nbrNum].nodeID == dvtable[nbrNum].dvEntry[i].nodeID)
      dvtable[nbrNum].dvEntry[i].cost = 0;
    else
      dvtable[nbrNum].dvEntry[i].cost = topology_getCost(dvtable[nbrNum].nodeID, dvtable[nbrNum].dvEntry[i].nodeID);
  }
  node_list_destroy(node_list);
  node_list_destroy(nbr_list);
  return dvtable;
}

//这个函数删除距离矢量表.
//它释放所有为距离矢量表动态分配的内存.
void dvtable_destroy(dv_t* dvtable)
{
  for(int i = 0; i < nbrNum + 1; i++)
  {
    free(dvtable[i].dvEntry);
  }
  free(dvtable);
  return;
}

//这个函数设置距离矢量表中2个节点之间的链路代价.
//如果这2个节点在表中发现了,并且链路代价也被成功设置了,就返回1,否则返回-1.
int dvtable_setcost(dv_t* dvtable,int fromNodeID,int toNodeID, unsigned int cost)
{
  for(int i = 0; i < nbrNum + 1; i++)
  {
    if(dvtable[i].nodeID == fromNodeID)
    {
      for(int j = 0; j < sumNum; j++)
      {
        if(dvtable[i].dvEntry[j].nodeID == toNodeID)
        {
          dvtable[i].dvEntry[j].cost = cost;
          return 1;
        }
      }
    }
  }
  return -1;
}

//这个函数返回距离矢量表中2个节点之间的链路代价.
//如果这2个节点在表中发现了,就返回链路代价,否则返回INFINITE_COST.
unsigned int dvtable_getcost(dv_t* dvtable, int fromNodeID, int toNodeID)
{
  for(int i = 0; i < nbrNum + 1; i++)
  {
    if(dvtable[i].nodeID == fromNodeID)
    {
      for(int j = 0; j < sumNum; j++)
      {
        if(dvtable[i].dvEntry[j].nodeID == toNodeID)
        {
          return dvtable[i].dvEntry[j].cost;
        }
      }
    }
  }
  return INFINITE_COST;
}

//这个函数打印距离矢量表的内容.
void dvtable_print(dv_t* dvtable)
{
  printf("DV Table:\n");
  for (int i = 0; i < nbrNum + 1; i++)
  {
    printf("Node %d:\n", dvtable[i].nodeID);
    for (int j = 0; j < sumNum; j++)
    {
      printf("  Node %d, Cost: %d\n", dvtable[i].dvEntry[j].nodeID, dvtable[i].dvEntry[j].cost);
    }
  }
  return;
}
