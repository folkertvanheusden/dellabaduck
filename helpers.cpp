#include <assert.h>
#include <ctype.h>
#include <stdlib.h>
#include <string>

#include "board.h"
#include "str.h"
#include "vertex.h"


extern Zobrist z;

board_t playerToStone(const player_t & p)
{
	return p == P_BLACK ? B_BLACK : B_WHITE;
}

player_t getOpponent(const player_t & p)
{
	return p == P_WHITE ? P_BLACK : P_WHITE;
}

std::string v2t(const Vertex & v)
{
	char xc = 'A' + v.getX();
	if (xc >= 'I')
		xc++;

	return myformat("%c%d", xc, v.getY() + 1);
}

Vertex t2v(const std::string & str, const int dim)
{
	char xc = tolower(str.at(0));
	assert(xc != 'i');
	if (xc >= 'j')
		xc--;
	int x = xc - 'a';

	int y = atoi(str.substr(1).c_str()) - 1;

	return { x, y, dim };
}

std::tuple<Board *, player_t, int> stringToPosition(const std::string & in)
{
	auto parts = split(in, " ");

	Board   *b      = new Board(&z, parts.at(0));

	player_t player = parts.at(1) == "b" || parts.at(1) == "B" ? P_BLACK : P_WHITE;

	int      pass   = atoi(parts.at(2).c_str());

	return { b, player, pass };
}

Board stringToBoard(const std::string & in)
{
	auto templines = split(in, "\n");

	for(size_t i=0; i<templines.size() / 2; i++) {
		std::string temp = templines.at(i);
		templines.at(i) = templines.at(templines.size() - i - 1);
		templines.at(templines.size() - i - 1) = temp;
	}

	auto work = merge(templines, "\n");

	const int dim = work.find("\n");
	Board b(&z, dim);

	int v = 0;

	for(auto c : work) {
		if (c == '.')
			b.setAt(v, B_EMPTY);
		else if (tolower(c) == 'x')
			b.setAt(v, B_BLACK);
		else if (tolower(c) == 'o')
			b.setAt(v, B_WHITE);
		else if (c == '\n')
			continue;

		v += 1;
	}

	assert(v == dim * dim);

	return b;
}

std::set<Vertex> stringToChain(const std::string & in, const int dim)
{
	auto parts = split(in, " ");

	std::set<Vertex> out;

	for(auto p : parts)
		out.insert(t2v(p, dim));

	return out;
}
