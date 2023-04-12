#include <tuple>

#include "board.h"


std::tuple<double, double, int, player_t> playout(const Board & in, const double komi, player_t p);
