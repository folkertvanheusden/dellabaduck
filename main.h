#include <optional>
#include <tuple>
#include <unordered_set>

#include "board.h"


std::tuple<double, double, int, std::optional<Vertex> > playout(const Board & in, const double komi, const board_t p, const std::unordered_set<uint64_t> & seen_in);
