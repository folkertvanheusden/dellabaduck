#include <set>
#include <string>
#include <unordered_set>
#include <vector>

#include "board.h"


void dump(const player_t p);
void dump(const std::set<Vertex> & set);
void dump(const std::unordered_set<Vertex, Vertex::HashFunction> & uset);
void dump(const std::vector<Vertex> & vector, const bool sorted = false);
void dump(const chain_t & chain);
void dump(const std::vector<chain_t *> & chains);
void dump(const Board & b);
std::string dumpToString(const Board & b, const player_t next_player, const int pass_depth);
std::string init_sgf(const int dim);
std::string dumpToSgf(const Board & b, const double komi, const bool with_end);
void dump(const Vertex & v);
