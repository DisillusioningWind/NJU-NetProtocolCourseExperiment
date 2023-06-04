#include "util.h"
#include "btdata.h"

announce_url_t* parse_announce_url(char* announce)
{
  char* announce_ind;
  char port_str[6];  // 端口号最大为5位数字
  int port_len = 0; // 端口号中的字符数
  int port;
  char* url;
  int url_len = 0;
  announce_ind = strstr(announce,"/announce");
  announce_ind--;
  while(*announce_ind != ':')
  {
    port_len++;
    announce_ind--;
  }
  strncpy(port_str,announce_ind+1,port_len);
  port_str[port_len+1] = '\0';
  port = atoi(port_str);
    
  char* p;
  for(p=announce; p<announce_ind; p++)
  {
    url_len++;   
  }

  announce_url_t* ret;
  ret = (announce_url_t*)malloc(sizeof(announce_url_t));
  if(ret == NULL)
  {
    perror("Could not allocate announce_url_t");
    exit(-73);
  }
 
  p = announce;
  printf("ANNOUNCE: %s\n",announce);
  if(strstr(announce,"http://") > 0)
  {
    url_len -= 7;
    p += 7;
  }
  
  url = (char*)malloc((url_len+1)*sizeof(char)); // +1 for \0
  strncpy(url,p,url_len);
  url[url_len+1] = '\0';
  
  ret->hostname = url;
  ret->port = port;

  return ret;
}

std::string SHA1toStr(unsigned int SHA1[5])
{
  std::string ret;
  char buf[9];
  for(int i=0; i<5; i++)
  {
    sprintf(buf,"%08x",SHA1[i]);
    ret += buf;
  }
  return ret;
}

std::vector<std::string> splitSHA1Str(char *SHA1str, int num_pieces)
{
  std::vector<std::string> ret;
  char buf[21];
  for(int i=0; i<num_pieces; i++)
  {
    strncpy(buf, SHA1str + i * 20, 20);
    buf[20] = '\0';
    ret.push_back(buf);
  }
  return ret;
}
