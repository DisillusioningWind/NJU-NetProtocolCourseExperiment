#ifndef _PIECE_H
#define _PIECE_H

#include <string>
#include <vector>

enum BlockStatus
{
    missing = 0,
    pending = 1,
    retrieved = 2
};

struct Block
{
    int piece;
    int offset;
    int length;
    BlockStatus status;
    std::string data;
};

class Piece
{
public:
    const std::string hashValue;
    const int index;
    std::vector<Block*> blocks;

    explicit Piece(int index, std::vector<Block *> blocks, std::string hashValue);
    ~Piece();
    void reset();
    std::string getData();
    Block* nextRequest();
    void blockReceived(int offset, std::string data);
    bool isComplete();
    bool isHashMatching();
};

#endif
