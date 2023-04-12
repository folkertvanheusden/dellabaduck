#include <string>

#include "board.h"
#include "vertex.h"


board_t playerToStone(const player_t & p);
player_t getOpponent(const player_t & p);
std::string v2t(const Vertex & v);
Vertex t2v(const std::string & str, const int dim);
std::tuple<Board *, player_t, int> stringToPosition(const std::string & in);
Board stringToBoard(const std::string & in);
std::set<Vertex> stringToChain(const std::string & in, const int dim);
