#ifndef _PWP_H
#define _PWP_H

#include "util.h"
#include "sharedqueue.h"
#include "piecemanager.h"

using byte = unsigned char;
typedef peerdata Peer;

enum MessageId
{
  keepAlive = -1,
  choke = 0,
  unchoke = 1,
  interested = 2,
  notInterested = 3,
  have = 4,
  bitField = 5,
  request = 6,
  piece = 7,
  cancel = 8,
  port = 9
};

class BitTorrentMessage
{
private:
  const uint32_t messageLength;
  const uint8_t id;
  const std::string payload;

public:
  explicit BitTorrentMessage(uint8_t id, const std::string &payload = "");
  std::string toString();
  uint8_t getMessageId() const;
  std::string getPayload() const;
};

class PeerConnection
{
private:
  int sock{};
  bool choked = true;
  bool terminated = false;
  bool requestPending = false;
  const std::string clientId;
  const std::string infoHash;
  SharedQueue<Peer *> *queue;
  Peer *peer;
  std::string peerBitField;
  std::string peerId;
  PieceManager *pieceManager;

  std::string createHandshakeMessage();
  void performHandshake();
  void receiveBitField();
  void sendInterested();
  void receiveUnchoke();
  void requestPiece();
  void closeSock();
  bool establishNewConnection();
  BitTorrentMessage receiveMessage(int bufferSize = 0) const;

public:
  const std::string &getPeerId() const;

  explicit PeerConnection(SharedQueue<Peer *> *queue, std::string clientId, std::string infoHash, PieceManager *pieceManager);
  ~PeerConnection();
  void start();
  void stop();
};


// void* pwp_thread(void* arg);

#endif