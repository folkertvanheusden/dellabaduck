#include <string>

#include "board.h"


std::string init_sgf(const int dim);
std::string dump_to_sgf(const Board & b, const double komi, const bool with_end);
