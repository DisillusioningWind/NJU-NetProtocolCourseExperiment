#ifndef TORRENTCLIENT_H
#define TORRENTCLIENT_H

#include <string>
#include "pwp.h"
#include "sharedqueue.h"

class TorrentClient
{
private:
  const int threadNum;
  std::string peerId;
  SharedQueue<Peer *> queue;
  std::vector<std::thread> threadPool;
  std::vector<PeerConnection *> connections;

public:
  explicit TorrentClient(int threadNum = 5, bool enableLogging = true);
  ~TorrentClient();
  void terminate();
  void downloadFile(const std::string &torrentFilePath, const std::string &downloadDirectory);
};

#endif