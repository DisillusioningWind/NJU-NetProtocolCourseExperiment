// 文件名: seg.c
// 描述: 这个文件包含用于发送和接收STCP段的接口sip_sendseg() and sip_rcvseg(), 及其支持函数的实现. 

#include "seg.h"
#include "stdio.h"

/// @brief 通过重叠网络(在本实验中，是一个TCP连接)发送STCP段
/// @param connection TCP连接描述符
/// @param segPtr 用于发送的STCP段指针
/// @retval 1 发送成功
/// @retval -1 发送失败，TCP连接出错，或TCP连接已关闭，或发送0字节数据
int sip_sendseg(int connection, seg_t* segPtr)
{
	// 计算校验和
	segPtr->header.checksum = checksum(segPtr);
	// printf("checksum: %d\n", segPtr->header.checksum);
	// 发送数据
	int res = 1;
	res = send(connection, "!&", 2, 0);
	res = send(connection, segPtr, sizeof(seg_t), 0);
	res = send(connection, "!#", 2, 0);
	return res > 0 ? 1 : -1;
}

#define SEGSTART1 0
#define SEGSTART2 1
#define SEGRECV1 2
#define SEGRECV2 3
#define SEGSTOP1 4
#define SEGSTOP2 5

/// @brief 通过重叠网络(在本实验中，是一个TCP连接)接收STCP段
/// @param connection TCP连接描述符
/// @param segPtr 用于接收的STCP段指针
/// @retval 1: 丢弃接收到的STCP段
/// @retval 0: 接收到正确的STCP段
/// @retval -1: TCP连接出错或已关闭
int sip_recvseg(int connection, seg_t* segPtr)
{
	int res = 1, check = 1;
	int state = SEGSTART1;
	char c;
	while (res > 0)
	{
		switch (state)
		{
		case SEGSTART1:
			res = recv(connection, &c, 1, 0);
			if (c == '!')
				state = SEGSTART2;
			break;
		case SEGSTART2:
			res = recv(connection, &c, 1, 0);
			if (c == '&')
				state = SEGRECV1;
			else
				state = SEGSTART1;
			break;
		case SEGRECV1:
			res = recv(connection, &(segPtr->header), sizeof(stcp_hdr_t), 0);
			state = SEGRECV2;
			break;
		case SEGRECV2:
			res = recv(connection, segPtr->data, MAX_SEG_LEN, 0);
			state = SEGSTOP1;
			break;
		case SEGSTOP1:
			res = recv(connection, &c, 1, 0);
			if (c == '!')
				state = SEGSTOP2;
			else
				return -1;
			break;
		case SEGSTOP2:
			res = recv(connection, &c, 1, 0);
			if (c == '#')
			{
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

int seglost(seg_t* segPtr) {
	int random = rand() % 100;
	if (random < PKT_LOSS_RATE * 100)
	{
		//50%可能性丢失段
		if (rand() % 2 == 0)
		{
			printf("seg lost\n");
			return 1;
		}
		//50%可能性是错误的校验和
		else {
			//获取数据长度
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
//校验和覆盖段首部和段数据. 你应该首先将段首部中的校验和字段清零, 
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

//这个函数检查段中的校验和, 正确时返回1, 错误时返回-1
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
