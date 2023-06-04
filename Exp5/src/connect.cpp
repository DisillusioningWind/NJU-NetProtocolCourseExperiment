#include <sys/socket.h>
#include <arpa/inet.h>
#include <stdexcept>
#include <cstring>
#include <iostream>
#include <chrono>
#include <netinet/in.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/poll.h>
#include <limits>

#include "util.h"

#define CONNECT_TIMEOUT 3
#define READ_TIMEOUT 3000 // 3 seconds

bool setSocketBlocking(int sock, bool blocking)
{
  if (sock < 0)
    return false;

  int flags = fcntl(sock, F_GETFL, 0);
  if (flags == -1)
    return false;
  flags = blocking ? (flags & ~O_NONBLOCK) : (flags | O_NONBLOCK);
  return (fcntl(sock, F_SETFL, flags) == 0);
}

int createConnection(const std::string &ip, const int port)
{
  int sock = 0;
  struct sockaddr_in address;
  if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    throw std::runtime_error("Socket creation error: " + std::to_string(sock));

  address.sin_family = AF_INET;
  address.sin_port = htons(port);

  char *tempIp = new char[ip.length() + 1];
  strcpy(tempIp, ip.c_str());

  // Converts IP address from string to struct in_addr
  if (inet_pton(AF_INET, tempIp, &address.sin_addr) <= 0)
    throw std::runtime_error("Invalid IP address: " + ip);

  // Sets socket to non-block mode
  if (!setSocketBlocking(sock, false))
    throw std::runtime_error("An error occurred when setting socket " + std::to_string(sock) + "to NONBLOCK");

  connect(sock, (struct sockaddr *)&address, sizeof(address));

  fd_set fdset;
  struct timeval tv;
  FD_ZERO(&fdset);
  FD_SET(sock, &fdset);
  tv.tv_sec = CONNECT_TIMEOUT;
  tv.tv_usec = 0;

  if (select(sock + 1, NULL, &fdset, NULL, &tv) == 1)
  {
    int so_error;
    socklen_t len = sizeof so_error;

    getsockopt(sock, SOL_SOCKET, SO_ERROR, &so_error, &len);

    if (so_error == 0)
    {
      // Sets socket to blocking mode
      if (!setSocketBlocking(sock, true))
        throw std::runtime_error("An error occurred when setting socket " + std::to_string(sock) + "to BLOCK");
      return sock;
    }
  }
  close(sock);
  throw std::runtime_error("Connect to " + ip + ": FAILED [Connection timeout]");
}

void sendData(const int sock, const std::string &data)
{
  int n = data.length();
  char buffer[n];
  for (int i = 0; i < n; i++)
    buffer[i] = data[i];

  int res = send(sock, buffer, n, 0);
  if (res < 0)
    throw std::runtime_error("Failed to write data to socket " + std::to_string(sock));
}

std::string receiveData(const int sock, uint32_t bufferSize)
{

  std::string reply;
  if (!bufferSize)
  {
    struct pollfd fd;
    int ret;
    fd.fd = sock;
    fd.events = POLLIN;
    ret = poll(&fd, 1, READ_TIMEOUT);

    long bytesRead;
    const int lengthIndicatorSize = 4;
    char buffer[lengthIndicatorSize];
    switch (ret)
    {
    case -1:
      throw std::runtime_error("Read failed from socket " + std::to_string(sock));
    case 0:
      throw std::runtime_error("Read timeout from socket " + std::to_string(sock));
    default:
      bytesRead = recv(sock, buffer, sizeof(buffer), 0);
    }
    if (bytesRead != lengthIndicatorSize)
      return reply;

    std::string messageLengthStr;
    for (char i : buffer)
      messageLengthStr += i;
    uint32_t messageLength = bytesToInt(messageLengthStr);
    bufferSize = messageLength;
  }

  if (bufferSize > std::numeric_limits<uint16_t>::max())
    throw std::runtime_error("Received corrupted data [Received buffer size greater than 2 ^ 16 - 1]");

  char buffer[bufferSize];
  long bytesRead = 0;
  long bytesToRead = bufferSize;
  auto startTime = std::chrono::steady_clock::now();
  do
  {
    auto diff = std::chrono::steady_clock::now() - startTime;
    if (std::chrono::duration<double, std::milli>(diff).count() > READ_TIMEOUT)
    {
      throw std::runtime_error("Read timeout from socket " + std::to_string(sock));
    }
    bytesRead = recv(sock, buffer, bufferSize, 0);

    if (bytesRead <= 0)
      throw std::runtime_error("Failed to receive data from socket " + std::to_string(sock));
    bytesToRead -= bytesRead;
    for (int i = 0; i < bytesRead; i++)
      reply.push_back(buffer[i]);
  } while (bytesToRead > 0);

  return reply;
}