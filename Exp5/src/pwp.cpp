#include <errno.h>
#include <iostream>
#include <sstream>
#include <bitset>
#include <assert.h>

#include "pwp.h"
#include "connect.h"

BitTorrentMessage::BitTorrentMessage(const uint8_t id, const std::string &payload) : id(id), payload(payload), messageLength(payload.length() + 1) {}

std::string BitTorrentMessage::toString()
{
  std::stringstream buffer;
  char *messageLengthAddr = (char *)&messageLength;
  std::string messageLengthStr;
  for (int i = 0; i < 4; i++)
    messageLengthStr.push_back((char)messageLengthAddr[3 - i]);
  buffer << messageLengthStr;
  buffer << (char)id;
  buffer << payload;
  return buffer.str();
}

uint8_t BitTorrentMessage::getMessageId() const
{
  return id;
}

std::string BitTorrentMessage::getPayload() const
{
  return payload;
}

#define INFO_HASH_STARTING_POS 28
#define PEER_ID_STARTING_POS 48
#define HASH_LEN 20
#define DUMMY_PEER_IP "0.0.0.0"

PeerConnection::PeerConnection(
    SharedQueue<Peer *> *queue,
    std::string clientId,
    std::string infoHash,
    PieceManager *pieceManager) : queue(queue), clientId(std::move(clientId)), infoHash(std::move(infoHash)), pieceManager(pieceManager)
{}

PeerConnection::~PeerConnection()
{
  closeSock();
}

void PeerConnection::start()
{
  // LOG_F(INFO, "Downloading thread started...");
  while (!(terminated || pieceManager->isComplete()))
  {
    peer = queue->pop_front();
    // Terminates the thread if it has received a dummy Peer
    if (peer->ip == DUMMY_PEER_IP)
      return;

    try
    {
      // Establishes connection with the peer, and lets it know
      // that we are interested.
      if (establishNewConnection())
      {
        while (!pieceManager->isComplete())
        {
          BitTorrentMessage message = receiveMessage();
          if (message.getMessageId() > 10)
            throw std::runtime_error("Received invalid message Id from peer " + peerId);
          switch (message.getMessageId())
          {
          case choke:
            choked = true;
            break;

          case unchoke:
            choked = false;
            break;

          case piece:
          {
            requestPending = false;
            std::string payload = message.getPayload();
            int index = bytesToInt(payload.substr(0, 4));
            int begin = bytesToInt(payload.substr(4, 4));
            std::string blockData = payload.substr(8);
            pieceManager->blockReceived(peerId, index, begin, blockData);
            break;
          }
          case have:
          {
            std::string payload = message.getPayload();
            int pieceIndex = bytesToInt(payload);
            pieceManager->updatePeer(peerId, pieceIndex);
            break;
          }

          default:
            break;
          }
          if (!choked)
          {
            if (!requestPending)
            {
              requestPiece();
            }
          }
        }
      }
    }
    catch (std::exception &e)
    {
      closeSock();
      // LOG_F(ERROR, "An error occurred while downloading from peer %s [%s]", peerId.c_str(), peer->ip.c_str());
      // LOG_F(ERROR, "%s", e.what());
    }
  }
}

void PeerConnection::stop()
{
  terminated = true;
}

void PeerConnection::performHandshake()
{
  // Connects to the peer
  // LOG_F(INFO, "Connecting to peer [%s]...", peer->ip.c_str());
  try
  {
    sock = createConnection(peer->ip, peer->port);
  }
  catch (std::runtime_error &e)
  {
    // throw std::runtime_error("Cannot connect to peer [" + peer->ip + "]");
  }
  // LOG_F(INFO, "Establish TCP connection with peer at socket %d: SUCCESS", sock);

  // Send the handshake message to the peer
  // LOG_F(INFO, "Sending handshake message to [%s]...", peer->ip.c_str());
  std::string handshakeMessage = createHandshakeMessage();
  sendData(sock, handshakeMessage);
  // LOG_F(INFO, "Send handshake message: SUCCESS");

  // Receive the reply from the peer
  // LOG_F(INFO, "Receiving handshake reply from peer [%s]...", peer->ip.c_str());
  std::string reply = receiveData(sock, handshakeMessage.length());
  if (reply.empty())
    throw std::runtime_error("Receive handshake from peer: FAILED [No response from peer]");
  peerId = reply.substr(PEER_ID_STARTING_POS, HASH_LEN);
  // LOG_F(INFO, "Receive handshake reply from peer: SUCCESS");

  // Compare the info hash from the peer's reply message with the info hash we sent.
  // If the two values are not the same, close the connection and raise an exception.
  std::string receivedInfoHash = reply.substr(INFO_HASH_STARTING_POS, HASH_LEN);
  // if ((receivedInfoHash == infoHash) != 0)
  //   throw std::runtime_error("Perform handshake with peer " + peer->ip + ": FAILED [Received mismatching info hash]");
  // LOG_F(INFO, "Hash comparison: SUCCESS");
}

void PeerConnection::receiveBitField()
{
  // Receive BitField from the peer
  // LOG_F(INFO, "Receiving BitField message from peer [%s]...", peer->ip.c_str());
  BitTorrentMessage message = receiveMessage();
  if (message.getMessageId() != bitField)
    throw std::runtime_error("Receive BitField from peer: FAILED [Wrong message ID]");
  peerBitField = message.getPayload();

  // Informs the PieceManager of the BitField received
  pieceManager->addPeer(peerId, peerBitField);

  // LOG_F(INFO, "Receive BitField from peer: SUCCESS");
}

void PeerConnection::requestPiece()
{
  Block *block = pieceManager->nextRequest(peerId);

  if (!block)
    return;

  int payloadLength = 12;
  char temp[payloadLength];
  // Needs to convert little-endian to big-endian
  uint32_t index = htonl(block->piece);
  uint32_t offset = htonl(block->offset);
  uint32_t length = htonl(block->length);
  memcpy(temp, &index, sizeof(int));
  memcpy(temp + 4, &offset, sizeof(int));
  memcpy(temp + 8, &length, sizeof(int));
  std::string payload;
  for (int i = 0; i < payloadLength; i++)
    payload += (char)temp[i];

  std::stringstream info;
  info << "Sending Request message to peer " << peer->ip << " ";
  info << "[Piece: " << std::to_string(block->piece) << " ";
  info << "Offset: " << std::to_string(block->offset) << " ";
  info << "Length: " << std::to_string(block->length) << "]";
  // LOG_F(INFO, "%s", info.str().c_str());
  std::string requestMessage = BitTorrentMessage(request, payload).toString();
  sendData(sock, requestMessage);
  requestPending = true;
  // LOG_F(INFO, "Send Request message: SUCCESS");
}

void PeerConnection::sendInterested()
{
  // LOG_F(INFO, "Sending Interested message to peer [%s]...", peer->ip.c_str());
  std::string interestedMessage = BitTorrentMessage(interested).toString();
  sendData(sock, interestedMessage);
  // LOG_F(INFO, "Send Interested message: SUCCESS");
}

void PeerConnection::receiveUnchoke()
{
  // LOG_F(INFO, "Receiving Unchoke message from peer [%s]...", peer->ip.c_str());
  BitTorrentMessage message = receiveMessage();
  if (message.getMessageId() != unchoke)
    throw std::runtime_error(
        "Receive Unchoke message from peer: FAILED [Wrong message ID: " +
        std::to_string(message.getMessageId()) + "]");
  choked = false;
  // LOG_F(INFO, "Receive Unchoke message: SUCCESS");
}

bool PeerConnection::establishNewConnection()
{
  try
  {
    performHandshake();
    receiveBitField();
    sendInterested();
    return true;
  }
  catch (const std::runtime_error &e)
  {
    // LOG_F(ERROR, "An error occurred while connecting with peer [%s]", peer->ip.c_str());
    // LOG_F(ERROR, "%s", e.what());
    return false;
  }
}

std::string PeerConnection::createHandshakeMessage()
{
  const std::string protocol = "BitTorrent protocol";
  std::stringstream buffer;
  buffer << (char)protocol.length();
  buffer << protocol;
  std::string reserved;
  for (int i = 0; i < 8; i++)
    reserved.push_back('\0');
  buffer << reserved;
  buffer << hexDecode(infoHash);
  buffer << clientId;
  assert(buffer.str().length() == protocol.length() + 49);
  return buffer.str();
}

BitTorrentMessage PeerConnection::receiveMessage(int bufferSize) const
{
  std::string reply = receiveData(sock, 0);
  if (reply.empty())
    return BitTorrentMessage(keepAlive);
  auto messageId = (uint8_t)reply[0];
  std::string payload = reply.substr(1);
  // LOG_F(INFO, "Received message with ID %d from peer [%s]", messageId, peer->ip.c_str());
  return BitTorrentMessage(messageId, payload);
}

const std::string &PeerConnection::getPeerId() const
{
  return peerId;
}

void PeerConnection::closeSock()
{
  if (sock)
  {
    // Close socket
    // LOG_F(INFO, "Closed connection at socket %d", sock);
    close(sock);
    sock = {};
    requestPending = false;
    // If the peer has been added to piece manager, remove it
    if (!peerBitField.empty())
    {
      peerBitField.clear();
      pieceManager->removePeer(peerId);
    }
  }
}