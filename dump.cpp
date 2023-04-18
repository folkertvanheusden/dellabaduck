#include <set>
#include <string>
#include <vector>

#include "board.h"
#include "helpers.h"
#include "io.h"
#include "str.h"


void dump(const player_t p)
{
	send(true, "# color: %s", p == P_BLACK ? "black" : "white");
}

void dump(const std::set<Vertex> & set)
{
	send(true, "# Vertex set");

	std::string line = "# ";
	for(auto v : set)
		line += myformat("%s ", v2t(v).c_str());
	send(true, line.c_str());
}

void dump(const std::vector<Vertex> & vector, const bool sorted)
{
	send(true, "# Vertex vector");

	auto vector_sorted = vector;

	if (sorted)
		std::sort(vector_sorted.begin(), vector_sorted.end());

	std::string line = "# ";

	for(auto v : vector_sorted)
		line += myformat("%s ", v2t(v).c_str());

	send(true, line.c_str());
}

void dump(const chain_t & chain)
{
	send(true, "# Chain for %s", board_t_name(chain.type));

	std::string line = "# ";
	for(auto v : chain.chain) 
		line += myformat("%s ", v2t(v).c_str());
	send(true, line.c_str());

	if (chain.liberties.empty() == false) {
		send(true, "# Liberties of that chain:");

		line = "# ";
		for(auto v : chain.liberties) 
			line += myformat("%s ", v2t(v).c_str());
		send(true, "%s", line.c_str());
	}
}

void dump(const std::vector<chain_t *> & chains)
{
	for(auto chain : chains)
		dump(*chain);
}

void dump(const Board & b)
{
	const int dim = b.getDim();

	std::string line;

	for(int y=dim - 1; y>=0; y--) {
		line = myformat("# %2d | ", y + 1);

		for(int x=0; x<dim; x++) {
			board_t bv = b.getAt(x, y);

			if (bv == B_EMPTY)
				line += ".";
			else if (bv == B_BLACK)
				line += "x";
			else if (bv == B_WHITE)
				line += "o";
			else
				line += "!";
		}

		send(true, "%s", line.c_str());
	}

	line = "#      ";

	for(int x=0; x<dim; x++) {
		int xc = 'A' + x;

		if (xc >= 'I')
			xc++;

		line += myformat("%c", xc);
	}

	send(true, "%s", line.c_str());
}

std::string dumpToString(const Board & b, const player_t next_player, const int pass_depth)
{
	const int   dim = b.getDim();

	std::string out;

	for(int y=dim - 1; y >= 0; y--) {
		for(int x=0; x<dim; x++) {
			auto stone = b.getAt(x, y);

			if (stone == B_EMPTY)
				out += ".";
			else if (stone == B_BLACK)
				out += "b";
			else
				out += "w";
		}

		if (y)
			out += "/";
	}

	out += next_player == P_WHITE ? " w" : " b";

	out += myformat(" %d", pass_depth);

	return out;
}

void dump(const ChainMap & cm)
{
	const int dim = cm.getDim();

	std::string line;

	for(int y=dim - 1; y>=0; y--) {
		line = myformat("# %2d | ", y + 1);

		for(int x=0; x<dim; x++) {
			auto p = cm.getAt(y * dim + x);

			if (p == nullptr)
				line += '.';
			else if (p->type == B_BLACK)
				line += 'x';
			else if (p->type == B_WHITE)
				line += 'o';
			else
				line += '!';
		}

		send(true, "%s", line.c_str());
	}

	line = "#      ";

	for(int x=0; x<dim; x++) {
		int xc = 'A' + x;

		if (xc >= 'I')
			xc++;

		line += myformat("%c", xc);
	}

	send(true, "%s", line.c_str());
}

std::string init_sgf(const int dim)
{
	return "(;AP[DellaBaduck]SZ[" + myformat("%d", dim) + "]";
}

std::string dumpToSgf(const Board & b, const double komi, const bool with_end)
{
	int         dim = b.getDim();
	std::string sgf = init_sgf(dim);

	sgf += myformat(";KM[%f]", komi);

	for(int y=0; y<dim; y++) {
		for(int x=0; x<dim; x++) {
			auto v     = Vertex(x, y, dim);

			auto stone = b.getAt(v.getV());

			if (stone == B_EMPTY)
				continue;

			sgf += myformat(";%c[%s]", stone == B_BLACK ? 'B' : 'W', v2t(v).c_str());
		}
	}

	return sgf + (with_end ? ")" : "");
}

void dumpDifference(const std::vector<Vertex> & a, const std::vector<Vertex> & b)
{
	auto acopy = a;
	auto bcopy = b;

	std::vector<Vertex> difference;

	std::sort(acopy.begin(), acopy.end());
	std::sort(bcopy.begin(), bcopy.end());

	std::set_difference(acopy.begin(), acopy.end(), bcopy.begin(), bcopy.end(), std::inserter(difference, difference.begin()));

	dump(difference, false);
}

void dump(const Vertex & v)
{
	printf("%s\n", v2t(v).c_str());
}
