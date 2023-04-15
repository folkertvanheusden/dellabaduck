#include <set>
#include <string>
#include <unordered_set>
#include <vector>

#include "board.h"
#include "vertex.h"


board_t playerToStone(const player_t & p);
player_t getOpponent(const player_t & p);
std::string v2t(const Vertex & v);
Vertex t2v(const std::string & str, const int dim);
std::tuple<Board *, player_t, int> stringToPosition(const std::string & in);
Board stringToBoard(const std::string & in);
std::set<Vertex> stringToChain(const std::string & in, const int dim);
bool compareChain(const std::set<Vertex> & a, const std::set<Vertex> & b);
bool compareChain(const std::unordered_set<Vertex, Vertex::HashFunction> & a, const std::unordered_set<Vertex, Vertex::HashFunction> & b);
bool compareChain(const std::unordered_set<Vertex, Vertex::HashFunction> & a, const std::set<Vertex> & b);
bool compareChain(const std::vector<Vertex> & a, const std::vector<Vertex> & b);
bool findChain(const std::vector<chain_t *> & chains, const std::vector<Vertex> & search_for);
bool findChain(const std::vector<chain_t *> & chains, const std::unordered_set<Vertex, Vertex::HashFunction> & search_for);
bool findChain(const std::vector<chain_t *> & chains, const std::set<Vertex> & search_for);
bool compareChainT(const std::vector<chain_t *> & chains1, const std::vector<chain_t *> & chains2);
