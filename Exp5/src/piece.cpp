#include <utility>
#include <vector>
#include <iostream>
#include <string>
#include <algorithm>
#include <sstream>
#include <cassert>

#include "sha1.h"
#include "piece.h"
#include "util.h"

Piece::Piece(int index,
             std::vector<Block *> blocks,
             std::string hashValue) : index(index), hashValue(std::move(hashValue))
{
    this->blocks = std::move(blocks);
}

Piece::~Piece()
{
    for (Block* block : blocks)
        delete block;
}

void Piece::reset()
{
    for (Block* block : blocks)
        block->status = missing;
}

Block* Piece::nextRequest()
{
    for (Block* block : blocks)
    {
        if (block->status == missing)
        {
            block->status = pending;
            return block;
        }
    }
    return nullptr;
}

void Piece::blockReceived(int offset, std::string data)
{
    for (Block* block : blocks)
    {
        if (block->offset == offset)
        {
            block->status = retrieved;
            block->data = data;
            return;
        }
    }
    throw std::runtime_error("receive wrong block,offset:" + std::to_string(offset) + ",index:" + std::to_string(index));
}

bool Piece::isComplete()
{
    return std::all_of(blocks.begin(), blocks.end(),[](Block* block)
       {
           return block->status == retrieved;
       }
    );
}

bool Piece::isHashMatching()
{
    std::string data = getData();
    SHA1Context sha;
    SHA1Reset(&sha);
    SHA1Input(&sha, reinterpret_cast<const unsigned char*>(data.c_str()), data.length());
    if (SHA1Result(&sha))
    {
        std::string hash = SHA1toStr(sha.Message_Digest);
        return hash == hashValue;
    }
    throw std::runtime_error("SHA1Result failed");
}

std::string Piece::getData()
{
    assert(isComplete());
    std::stringstream data;
    for (Block* block : blocks)
        data << block->data;
    return data.str();
}





