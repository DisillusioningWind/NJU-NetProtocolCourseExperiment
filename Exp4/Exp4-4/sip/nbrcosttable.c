
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <string.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include "nbrcosttable.h"
#include "../common/constants.h"
#include "../topology/topology.h"

//这个函数动态创建邻居代价表并使用邻居节点ID和直接链路代价初始化该表.
//邻居的节点ID和直接链路代价提取自文件topology.dat. 
nbr_cost_entry_t* nbrcosttable_create()
{
  node_list_t* nbr_list = topology_getNbrArray();
  nbr_cost_entry_t* nct = (nbr_cost_entry_t*)malloc(sizeof(nbr_cost_entry_t) * nbr_list->nodeNum);
  for (int i = 0; i < nbr_list->nodeNum; i++)
  {
    nct[i].nodeID = nbr_list->IDArr[i];
    nct[i].cost = nbr_list->costArr[i];
  }
  node_list_destroy(nbr_list);
  return nct;
}

//这个函数删除邻居代价表.
//它释放所有用于邻居代价表的动态分配内存.
void nbrcosttable_destroy(nbr_cost_entry_t* nct)
{
  if(nct == NULL) return;
  free(nct);
}

//这个函数用于获取邻居的直接链路代价.
//如果邻居节点在表中发现,就返回直接链路代价.不是邻居返回-1
unsigned int nbrcosttable_getcost(nbr_cost_entry_t* nct, int nodeID)
{
  for (int i = 0; i < nbrNum; i++)
  {
    if (nct[i].nodeID == nodeID)
      return nct[i].cost;
  }
  return -1;
}

int nbrcosttable_setcost(nbr_cost_entry_t *nct, int nodeID, unsigned int cost)
{
  for (int i = 0; i < nbrNum; i++)
  {
    if (nct[i].nodeID == nodeID)
    {
      nct[i].cost = cost;
      return 1;
    }
  }
  return -1;
}

//这个函数打印邻居代价表的内容.
void nbrcosttable_print(nbr_cost_entry_t* nct)
{
  printf("Neighbor Cost Table:\n");
  printf("Destination\tCost\n");
  for (int i = 0; i < nbrNum; i++)
    printf("%d\t%d\n", nct[i].nodeID, nct[i].cost);
}
