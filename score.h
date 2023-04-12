#include <string>
#include <utility>

#include "board.h"


std::pair<double, double> score(const Board & b, const double komi);
std::string scoreStr(const std::pair<double, double> & scores);
