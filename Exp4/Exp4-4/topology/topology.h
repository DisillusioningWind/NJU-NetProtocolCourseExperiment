//文件名: topology/topology.h
//
//描述: 这个文件声明一些用于解析拓扑文件的辅助函数 

#ifndef TOPOLOGY_H 
#define TOPOLOGY_H

#include <unistd.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>


typedef struct node_list {
    int nodeNum;      //节点数
    int* IDArr;       //节点ID数组
    in_addr_t* IPArr; //节点IP地址数组
    int* costArr;     //节点直接链路代价数组
} node_list_t;

extern int sumNum;  //节点数
extern int nbrNum;  //邻居数
extern int myID;    //本机节点ID

//这个函数初始化拓扑信息.设置sumNum和nbrNum.
int topology_init();

//这个函数返回指定主机的节点ID.
//节点ID是节点IP地址最后8位表示的整数.
//例如, 一个节点的IP地址为202.119.32.12, 它的节点ID就是12.
//如果不能获取节点ID, 返回-1.
int topology_getNodeIDfromname(char* hostname);

//这个函数返回指定主机的节点ID和IP地址.
int topology_getIPIDfromname(char *hostname, in_addr_t *addr, int *id);

// 这个函数返回指定的IP地址的节点ID.
// 如果不能获取节点ID, 返回-1.
int topology_getNodeIDfromip(struct in_addr *addr);

//这个函数返回本机的节点ID
//如果不能获取本机的节点ID, 返回-1.
int topology_getMyNodeID();

//这个函数解析保存在文件topology.dat中的拓扑信息.
//返回邻居数.
int topology_getNbrNum(); 

//这个函数解析保存在文件topology.dat中的拓扑信息.
//返回重叠网络中的总节点数.
int topology_getNodeNum();

//这个函数解析保存在文件topology.dat中的拓扑信息.
//返回一个动态分配的数组, 它包含重叠网络中所有节点的ID.
node_list_t* topology_getNodeArray();

//这个函数解析保存在文件topology.dat中的拓扑信息.
//返回一个动态分配的数组, 它包含所有邻居的节点ID.
node_list_t* topology_getNbrArray();
//用于为SON进程的nt表提供邻居的节点ID和IP地址
int topology_getNbrArray_for_nt(int ids[], in_addr_t ips[], int *num);

// 这个函数解析保存在文件topology.dat中的拓扑信息.
// 返回指定两个节点之间的直接链路代价.
// 如果指定两个节点之间没有直接链路, 返回INFINITE_COST.
unsigned int topology_getCost(int fromNodeID, int toNodeID);

//这个函数释放node_list_t分配的内存.
void node_list_destroy(node_list_t* list);
#endif
