//文件名: topology/topology.c
//
//描述: 这个文件实现一些用于解析拓扑文件的辅助函数 

#include "topology.h"
#include "../common/constants.h"

//这个函数返回指定主机的节点ID.
//节点ID是节点IP地址最后8位表示的整数.
//例如, 一个节点的IP地址为202.119.32.12, 它的节点ID就是12.
//如果不能获取节点ID, 返回-1.
int topology_getNodeIDfromname(char* hostname) 
{
  struct hostent* host = gethostbyname(hostname);
  if (host == NULL) return -1;
  uint32_t addr = ntohl(*((uint32_t *)host->h_addr_list[0]));
  int last = (addr & 0xff);
  printf("getNodeID:%d\n",last);
  return last;
}

//这个函数返回指定的IP地址的节点ID.
//如果不能获取节点ID, 返回-1.
int topology_getNodeIDfromip(struct in_addr* addr)
{
  uint32_t addr_net = ntohl(addr->s_addr);
  int last = addr_net & 0xff;
  printf("getNodeID:%d\n", last);
  return last;
}

int topology_getIDIPfromname(char *hostname, in_addr_t *addr, int *id)
{
  struct hostent *host = gethostbyname(hostname);
  if (host == NULL) return -1;
  uint32_t addr_net = ntohl(*((uint32_t *)host->h_addr_list[0]));
  *addr = addr_net;
  *id = addr_net & 0xff;
  return 0;
}

//这个函数返回本机的节点ID
//如果不能获取本机的节点ID, 返回-1.
int topology_getMyNodeID()
{
  char hostname[MAX_NAME_LEN];
  gethostname(hostname, sizeof(hostname));
  return topology_getNodeIDfromname(hostname);
}

//这个函数解析保存在文件topology.dat中的拓扑信息.
//返回邻居数.
int topology_getNbrNum()
{
  char hostname[MAX_NAME_LEN];
  gethostname(hostname, sizeof(hostname));
  FILE* fp = fopen("topology.dat", "r");
  if (fp == NULL) return -1;
  int nbrNum = 0;
  char line[MAX_LINE_LEN];
  while (fgets(line, sizeof(line), fp) != NULL) {
    char* p = strtok(line, " ");
    if (strcmp(p, hostname) == 0) nbrNum++;
    p = strtok(NULL, " ");
    if (strcmp(p, hostname) == 0) nbrNum++;
  }
  fclose(fp);
  return nbrNum;
}

void add_if_notFind(char nodes[][MAX_NAME_LEN], int* num_nodes, char* node)
{
  int i = 0;
  for (; i < *num_nodes; i++)
  {
    if (strcmp(nodes[i], node) == 0) break;
  }
  if(i == *num_nodes)
  {
    strcpy(nodes[i], node);
    (*num_nodes)++;
  }
}

//这个函数解析保存在文件topology.dat中的拓扑信息.
//返回重叠网络中的总节点数.
int topology_getNodeNum()
{
  //topology.dat数据格式为 hostname1 hostname2 cost，你要计算的总节点数不算重复的
  char filename[] = "topology.dat";
  char line[MAX_LINE_LEN];
  char nodes[MAX_NODE_NUM][MAX_NAME_LEN];
  char node1[MAX_NAME_LEN], node2[MAX_NAME_LEN];
  int cost;
  int num_nodes = 0;

  FILE *fp = fopen(filename, "r");
  if (fp == NULL) return -1;
  while (fgets(line, sizeof(line), fp) != NULL)
  {
    if (sscanf(line, "%s %s %d", node1, node2, &cost) == 3)
    {
      add_if_notFind(nodes, &num_nodes, node1);
      add_if_notFind(nodes, &num_nodes, node2);
    }
  }
  fclose(fp);
  printf("Total number of nodes: %d\n", num_nodes);
  return num_nodes;
}

//这个函数解析保存在文件topology.dat中的拓扑信息.
//返回一个动态分配的数组, 它包含重叠网络中所有节点的ID. 
int* topology_getNodeArray()
{
  char filename[] = "topology.dat";
  char line[MAX_LINE_LEN];
  char nodes[MAX_NODE_NUM][MAX_NAME_LEN];
  char node1[MAX_NAME_LEN], node2[MAX_NAME_LEN];
  int cost;
  int num_nodes = 0;

  FILE *fp = fopen(filename, "r");
  if (fp == NULL) return NULL;
  while (fgets(line, sizeof(line), fp) != NULL)
  {
    if (sscanf(line, "%s %s %d", node1, node2, &cost) == 3)
    {
      add_if_notFind(nodes, &num_nodes, node1);
      add_if_notFind(nodes, &num_nodes, node2);
    }
  }
  fclose(fp);

  int* nodeArray = (int*)malloc(sizeof(int) * num_nodes);
  for (int i = 0; i < num_nodes; i++)
  {
    nodeArray[i] = topology_getNodeIDfromname(nodes[i]);
  }
  return nodeArray;
}

//这个函数解析保存在文件topology.dat中的拓扑信息.
//返回一个动态分配的数组, 它包含所有邻居的节点ID.  
int topology_getNbrArray(int* ids, in_addr_t* ips, int* num)
{
  char filename[] = "topology.dat";
  char line[MAX_LINE_LEN];
  char nodes[MAX_NODE_NUM][MAX_NAME_LEN];
  char node1[MAX_NAME_LEN], node2[MAX_NAME_LEN];
  int cost;
  int num_nodes = 0;
  char hostname[MAX_NAME_LEN];
  gethostname(hostname, sizeof(hostname));

  FILE *fp = fopen("topology.dat", "r");
  if (fp == NULL) return -1;
  while (fgets(line, sizeof(line), fp) != NULL)
  {
    if (sscanf(line, "%s %s %d", node1, node2, &cost) == 3)
    {
      if (strcmp(node1, hostname) == 0)
      {
        strcpy(nodes[num_nodes], node2);
        num_nodes++;
      }
      else if (strcmp(node2, hostname) == 0)
      {
        strcpy(nodes[num_nodes], node1);
        num_nodes++;
      }
    }
  }
  fclose(fp);

  ids = (int *)malloc(sizeof(int) * num_nodes);
  ips = (in_addr_t *)malloc(sizeof(in_addr_t) * num_nodes);
  *num = num_nodes;
  for (int i = 0; i < num_nodes; i++)
  {
    topology_getIDIPfromname(nodes[i], &ids[i], &ips[i]);
  }
  return 0;
}

//这个函数解析保存在文件topology.dat中的拓扑信息.
//返回指定两个节点之间的直接链路代价. 
//如果指定两个节点之间没有直接链路, 返回INFINITE_COST.
unsigned int topology_getCost(int fromNodeID, int toNodeID)
{
  return 0;
}
