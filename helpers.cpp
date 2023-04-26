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

bool compareChain(const std::set<Vertex> & a, const std::set<Vertex> & b)
{
	if (a.size() != b.size())
		return false;

	for(auto v : a) {
		if (b.find(v) == b.end())
			return false;
	}

	return true;
}

bool compareChain(const std::unordered_set<Vertex, Vertex::HashFunction> & a, const std::unordered_set<Vertex, Vertex::HashFunction> & b)
{
	if (a.size() != b.size())
		return false;

	for(auto v : a) {
		if (b.find(v) == b.end())
			return false;
	}

	return true;
}

bool compareChain(const std::unordered_set<Vertex, Vertex::HashFunction> & a, const std::set<Vertex> & b)
{
	if (a.size() != b.size())
		return false;

	for(auto v : a) {
		if (b.find(v) == b.end())
			return false;
	}

	return true;
}

bool compareChain(const std::vector<Vertex> & a, const std::vector<Vertex> & b)
{
	if (a.size() != b.size())
		return false;

	for(auto v : a) {
		if (std::find(b.begin(), b.end(), v) == b.end())
			return false;
	}

	return true;
}

bool findChain(const std::vector<chain_t *> & chains, const std::vector<Vertex> & search_for)
{
	for(auto c : chains) {
		if (compareChain(c->chain, search_for))
			return true;
	}

	return false;
}

bool findChain(const std::vector<chain_t *> & chains, const std::unordered_set<Vertex, Vertex::HashFunction> & search_for)
{
	for(auto c : chains) {
		if (compareChain(c->liberties, search_for))
			return true;
	}

	return false;
}

bool findChain(const std::vector<chain_t *> & chains, const std::set<Vertex> & search_for)
{
	for(auto c : chains) {
		if (compareChain(c->liberties, search_for))
			return true;
	}

	return false;
}

bool compareChainT(const std::vector<chain_t *> & chains1, const std::vector<chain_t *> & chains2)
{
	for(auto & c : chains1) {
		if (findChain(chains2, c->chain) == false)
			return false;

		if (findChain(chains2, c->liberties) == false)
			return false;
	}

	return true;
}
