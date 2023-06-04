#ifndef _PIECEMANAGER_H
#define _PIECEMANAGER_H

#include <map>
#include <vector>
#include <ctime>
#include <mutex>
#include <fstream>
#include <thread>

#include "piece.h"
#include "util.h"

struct PendingRequest
{
    Block* block;
    time_t timestamp;
};

class PieceManager
{
private:
    std::map<std::string, std::string> peers;
    std::vector<Piece*> missingPieces;
    std::vector<Piece*> ongoingPieces;
    std::vector<Piece*> havePieces;
    std::vector<PendingRequest*> pendingRequests;
    std::ofstream downloadedFile;
    // std::thread& progressTrackerThread;
    const long pieceLength;
    const int maximumConnections;
    int piecesDownloadedInInterval = 0;
    time_t startingTime;
    int totalPieces{};

    std::mutex lock;

    std::vector<Piece*> initiatePieces();
    Block* expiredRequest(std::string peerId);
    Block* nextOngoing(std::string peerId);
    Piece* getRarestPiece(std::string peerId);
    void write(Piece* piece);
    void displayProgressBar();
    void trackProgress();
public:
    explicit PieceManager(const std::string& downloadPath, int maximumConnections);
    ~PieceManager();
    bool isComplete();
    void blockReceived(std::string peerId, int pieceIndex, int blockOffset, std::string data);
    void addPeer(const std::string& peerId, std::string bitField);
    void removePeer(const std::string& peerId);
    void updatePeer(const std::string& peerId, int index);
    unsigned long bytesDownloaded();
    Block* nextRequest(std::string peerId);
};

#endif
