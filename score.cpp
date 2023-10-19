#include <string>
#include <utility>

#include "board.h"
#include "str.h"


void scoreFloodFill(const Board & b, const int dim, bool *const reachable, const int x, const int y, const board_t lookFor)
{
	int v = y * dim + x;

	auto piece = b.getAt(v);

	if (piece != lookFor || reachable[v])
		return;

	reachable[v] = true;

	const int dimm1 = dim - 1;

	if (x > 0)
		scoreFloodFill(b, dim, reachable, x - 1, y, lookFor);
	if (x < dimm1)
		scoreFloodFill(b, dim, reachable, x + 1, y, lookFor);

	if (y > 0)
		scoreFloodFill(b, dim, reachable, x, y - 1, lookFor);
	if (y < dimm1)
		scoreFloodFill(b, dim, reachable, x, y + 1, lookFor);
}

// black, white
std::pair<double, double> score(const Board & b, const double komi)
{
	const int dim   = b.getDim();
	const int dimsq = dim * dim;

	int blackStones = 0;
	int whiteStones = 0;
	bool *reachableBlack = new bool[dimsq]();
	bool *reachableWhite = new bool[dimsq]();

	for(int y=0; y<dim; y++) {
		for(int x=0; x<dim; x++) {
			auto piece = b.getAt(x, y);

			if (piece == board_t::B_BLACK)
				scoreFloodFill(b, dim, reachableBlack, x, y, board_t::B_BLACK);
			else if (piece == board_t::B_WHITE)
				scoreFloodFill(b, dim, reachableWhite, x, y, board_t::B_WHITE);
		}
	}

	int blackEmpty = 0;
	int whiteEmpty = 0;

	for(int i=0; i<dimsq; i++) {
		if (reachableBlack[i] == true && reachableWhite[i] == false)
			blackEmpty++;
		else if (reachableWhite[i] == true && reachableBlack[i] == false)
			whiteEmpty++;
	}

	delete [] reachableBlack;
	delete [] reachableWhite;

	double blackScore = blackStones + blackEmpty;
	double whiteScore = whiteStones + whiteEmpty + komi;

	return { blackScore, whiteScore };
}

std::string scoreStr(const std::pair<double, double> & scores)
{
	if (scores.first > scores.second)
		return myformat("B+%g", scores.first - scores.second);

	if (scores.first < scores.second)
		return myformat("W+%g", scores.second - scores.first);

	return "0";
}

double score(const Board & b, const double komi, const board_t player)
{
	auto s = score(b, komi);

	return player == board_t::B_BLACK ? s.first - s.second : s.second - s.first;
}
