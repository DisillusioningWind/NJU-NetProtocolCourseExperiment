//文件名: topology/topology.c
//
//描述: 这个文件实现一些用于解析拓扑文件的辅助函数 

#include "topology.h"
#include "../common/constants.h"

int sumNum = -1;
int nbrNum = -1;
int myID = -1;

int topology_init()
{
  sumNum = topology_getNodeNum();
  nbrNum = topology_getNbrNum();
  myID = topology_getMyNodeID();
  return 0;
}

//这个函数返回指定主机的节点ID.
//节点ID是节点IP地址最后8位表示的整数.
//例如, 一个节点的IP地址为202.119.32.12, 它的节点ID就是12.
//如果不能获取节点ID, 返回-1.
int topology_getNodeIDfromname(char* hostname) 
{
  struct hostent *host = gethostbyname(hostname);
  if (host != NULL)
  {
    char **pptr = host->h_addr_list;
    uint32_t addr;
    int last;
    for (; *pptr != NULL; pptr++)
    {
      addr = ntohl(*((uint32_t *)(*pptr)));
      last = (addr & 0xff);
      if (last != 1)
        return last;
    }
  }
  printf("Can't get host by name, hostname:%s\n", hostname);
  return -1;
}

int topology_getIPIDfromname(char *hostname, in_addr_t *addr, int *id)
{
  struct hostent *host = gethostbyname(hostname);
  if (host == NULL)
  {
    printf("Can't get host by name, hostname:%s\n", hostname);
    return -1;
  }
  uint32_t addr_net = ntohl(*((uint32_t *)host->h_addr_list[0]));
  *addr = *((uint32_t *)host->h_addr_list[0]);
  *id = addr_net & 0xff;
  return 0;
}

//这个函数返回指定的IP地址的节点ID.
//如果不能获取节点ID, 返回-1.
int topology_getNodeIDfromip(struct in_addr* addr)
{
  uint32_t addr_net = ntohl(addr->s_addr);
  int last = addr_net & 0xff;
  return last;
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
  FILE *fp = fopen(TOPOLOGY_FILE, "r");
  if (fp == NULL) return -1;
  int tmpNum = 0;
  char line[MAX_LINE_LEN];
  while (fgets(line, sizeof(line), fp) != NULL)
  {
    char *p = strtok(line, " ");
    if (strcmp(p, hostname) == 0) tmpNum++;
    p = strtok(NULL, " ");
    if (strcmp(p, hostname) == 0) tmpNum++;
  }
  fclose(fp);
  return tmpNum;
}

void add_if_notFind(char nodes[][MAX_NAME_LEN], int *num_nodes, char *node)
{
  int i = 0;
  for (; i < *num_nodes; i++)
  {
    if (strcmp(nodes[i], node) == 0) break;
  }
  if (i == *num_nodes)
  {
    strcpy(nodes[i], node);
    (*num_nodes)++;
  }
}

//这个函数解析保存在文件topology.dat中的拓扑信息.
//返回重叠网络中的总节点数.
int topology_getNodeNum()
{
  char line[MAX_LINE_LEN];
  char nodes[MAX_NODE_NUM][MAX_NAME_LEN];
  char node1[MAX_NAME_LEN], node2[MAX_NAME_LEN];
  int cost;
  int num_nodes = 0;

  FILE *fp = fopen(TOPOLOGY_FILE, "r");
  if (fp == NULL)
    return -1;
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
node_list_t* topology_getNodeArray()
{
  char line[MAX_LINE_LEN];
  char nodes[MAX_NODE_NUM][MAX_NAME_LEN];
  char node1[MAX_NAME_LEN], node2[MAX_NAME_LEN];
  int cost;
  node_list_t* node_list = (node_list_t *)malloc(sizeof(node_list_t));
  node_list->nodeNum = 0;
  node_list->IDArr = NULL;
  node_list->IPArr = NULL;
  node_list->costArr = NULL;

  FILE *fp = fopen(TOPOLOGY_FILE, "r");
  if (fp == NULL) return node_list;
  while (fgets(line, sizeof(line), fp) != NULL)
  {
    if (sscanf(line, "%s %s %d", node1, node2, &cost) == 3)
    {
      add_if_notFind(nodes, &node_list->nodeNum, node1);
      add_if_notFind(nodes, &node_list->nodeNum, node2);
    }
  }
  fclose(fp);

  node_list->IDArr = (int *)malloc(sizeof(int) * node_list->nodeNum);
  for (int i = 0; i < node_list->nodeNum; i++)
    node_list->IDArr[i] = topology_getNodeIDfromname(nodes[i]);
  return node_list;
}

//这个函数解析保存在文件topology.dat中的拓扑信息.
//返回一个动态分配的数组, 它包含所有邻居的节点ID.
node_list_t* topology_getNbrArray()
{
  char nodes[MAX_NODE_NUM][MAX_NAME_LEN];
  int costs[MAX_NODE_NUM];
  char line[MAX_LINE_LEN];
  char node1[MAX_NAME_LEN], node2[MAX_NAME_LEN];
  int cost;
  node_list_t* list = (node_list_t *)malloc(sizeof(node_list_t));
  list->nodeNum = 0;
  list->IDArr = NULL;
  list->IPArr = NULL;
  list->costArr = NULL;
  char hostname[MAX_NAME_LEN];
  gethostname(hostname, sizeof(hostname));

  FILE *fp = fopen(TOPOLOGY_FILE, "r");
  if (fp == NULL) return list;
  while (fgets(line, sizeof(line), fp) != NULL)
  {
    if (sscanf(line, "%s %s %d", node1, node2, &cost) == 3)
    {
      if (strcmp(node1, hostname) == 0)
      {
        strcpy(nodes[list->nodeNum], node2);
        costs[list->nodeNum] = cost;
        list->nodeNum++;
      }
      else if (strcmp(node2, hostname) == 0)
      {
        strcpy(nodes[list->nodeNum], node1);
        costs[list->nodeNum] = cost;
        list->nodeNum++;
      }
    }
  }
  fclose(fp);

  list->IDArr = (int *)malloc(sizeof(int) * list->nodeNum);
  list->IPArr = (in_addr_t *)malloc(sizeof(in_addr_t) * list->nodeNum);
  list->costArr = (int *)malloc(sizeof(int) * list->nodeNum);
  for (int i = 0; i < list->nodeNum; i++)
  {
    topology_getIPIDfromname(nodes[i], &list->IPArr[i], &list->IDArr[i]);
    list->costArr[i] = costs[i];
  }
  return list;
}

int topology_getNbrArray_for_nt(int ids[], in_addr_t ips[], int *num)
{
  char line[MAX_LINE_LEN];
  char nodes[MAX_NODE_NUM][MAX_NAME_LEN];
  char node1[MAX_NAME_LEN], node2[MAX_NAME_LEN];
  int cost;
  int num_nodes = 0;
  char hostname[MAX_NAME_LEN];
  gethostname(hostname, sizeof(hostname));

  FILE *fp = fopen(TOPOLOGY_FILE, "r");
  if (fp == NULL)
    return -1;
  printf("Your hostname: %s\n", hostname);
  while (fgets(line, sizeof(line), fp) != NULL)
  {
    if (sscanf(line, "%s %s %d", node1, node2, &cost) == 3)
    {
      // printf("%s %s %d\n", node1, node2, cost);
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

  *num = num_nodes;
  for (int i = 0; i < num_nodes; i++)
  {
    topology_getIPIDfromname(nodes[i], &ips[i], &ids[i]);
  }
  return 0;
}

//这个函数解析保存在文件topology.dat中的拓扑信息.
//返回指定两个节点之间的直接链路代价. 
//如果指定两个节点之间没有直接链路, 返回INFINITE_COST.
unsigned int topology_getCost(int fromNodeID, int toNodeID)
{
  char line[MAX_LINE_LEN];
  char node1[MAX_NAME_LEN], node2[MAX_NAME_LEN];
  int cost;
  
  FILE *fp = fopen(TOPOLOGY_FILE, "r");
  if (fp == NULL) return INFINITE_COST;
  while (fgets(line, sizeof(line), fp) != NULL)
  {
    if (sscanf(line, "%s %s %d", node1, node2, &cost) == 3)
    {
      int nodeID1 = topology_getNodeIDfromname(node1);
      int nodeID2 = topology_getNodeIDfromname(node2);
      if ((nodeID1 == fromNodeID && nodeID2 == toNodeID) || (nodeID1 == toNodeID && nodeID2 == fromNodeID))
      {
        fclose(fp);
        return cost;
      }
    }
  }
  fclose(fp);
  return INFINITE_COST;
}

void node_list_destroy(node_list_t *list)
{
  if (list == NULL) return;
  if (list->IDArr != NULL) free(list->IDArr);
  if (list->IPArr != NULL) free(list->IPArr);
  if (list->costArr != NULL) free(list->costArr);
  free(list);
}
