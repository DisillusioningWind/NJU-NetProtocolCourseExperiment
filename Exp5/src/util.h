
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <string.h>
#include <string>
#include <vector>
#include <cmath>
#include <bitset>

#include "btdata.h"
#include "bencode.h"

#ifndef UTIL_H
#define UTIL_H

#define MAXLINE 4096

int is_bigendian();

// 从一个已连接套接字接收数据的函数
int recvline(int fd, char **line);
int recvlinef(int fd, char *format, ...);

// 连接到另一台主机, 返回sockfd
int connect_to_host(char* ip, int port);

// 监听指定端口, 返回监听套接字
int make_listen_port(int port);

// 返回文件的长度, 单位为字节
int file_len(FILE* fname);

// 从torrent文件中提取数据
torrentmetadata_t* parsetorrentfile(char* filename);

// 从Tracker响应中提取有用的数据
tracker_response* preprocess_tracker_response(int sockfd);

// 从Tracker响应中提取peer连接信息
tracker_data* get_tracker_data(char* data, int len);
void get_peers(tracker_data* td, be_node* peer_list); // 上面函数的辅助函数
void get_peer_data(peerdata* peer, be_node* ben_res); // 上面函数的辅助函数

// 制作一个发送给Tracker的HTTP请求, 返回该字符串
char* make_tracker_request(int event, int* mlen);

// 处理来自peer的整数的辅助函数
int reverse_byte_orderi(int i);
int make_big_endian(int i);
int make_host_orderi(int i);

// ctrl-c信号的处理函数
void client_shutdown(int sig);

// 从announce url中提取主机和端口数据
announce_url_t* parse_announce_url(char* announce);

// 将SHA1哈希值转换为字符串
std::string SHA1toStr(unsigned int SHA1[5]);

std::vector<std::string> splitSHA1Str(char* SHA1str, int num_pieces);

void setPiece(std::string &bitField, int index)
{
  int byteIndex = floor(index / 8);
  int offset = index % 8;
  bitField[byteIndex] |= (1 << (7 - offset));
}

bool hasPiece(const std::string &bitField, int index)
{
  int byteIndex = floor(index / 8);
  int offset = index % 8;
  return (bitField[byteIndex] >> (7 - offset) & 1) != 0;
}

std::string formatTime(long seconds)
{
  if (seconds < 0)
    return "inf";

  std::string result;
  // compute h, m, s
  std::string h = std::to_string(seconds / 3600);
  std::string m = std::to_string((seconds % 3600) / 60);
  std::string s = std::to_string(seconds % 60);
  // add leading zero if needed
  std::string hh = std::string(2 - h.length(), '0') + h;
  std::string mm = std::string(2 - m.length(), '0') + m;
  std::string ss = std::string(2 - s.length(), '0') + s;
  // return mm:ss if hh is 00
  if (hh.compare("00") != 0)
  {
    result = hh + ':' + mm + ":" + ss;
  }
  else
  {
    result = mm + ":" + ss;
  }
  return result;
}

int bytesToInt(std::string bytes)
{
  // FIXME: Use bitwise operation to convert
  std::string binStr;
  long byteCount = bytes.size();
  for (int i = 0; i < byteCount; i++)
    binStr += std::bitset<8>(bytes[i]).to_string();
  return stoi(binStr, 0, 2);
}

std::string hexDecode(const std::string &value)
{
  int hashLength = value.length();
  std::string decodedHexString;
  for (int i = 0; i < hashLength; i += 2)
  {
    std::string byte = value.substr(i, 2);
    char c = (char)(int)strtol(byte.c_str(), nullptr, 16);
    decodedHexString.push_back(c);
  }
  return decodedHexString;
}

#endif
