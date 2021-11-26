#include <assert.h>
#include <ctype.h>
#include <fstream>
#include <optional>
#include <set>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <vector>

#include "str.h"

typedef enum { P_BLACK, P_WHITE } player_t;
typedef enum { B_EMPTY, B_WHITE, B_BLACK, B_IGNORE } board_t;

class Vertex
{
private:
	const int v, dim;

public:
//	Vertex(const int v, const int dim) : v(v), dim(dim) {
//	}

	Vertex(const int x, const int y, const int dim) : v(y * dim + x), dim(dim) {
	}

	Vertex(const Vertex & v) : v(v.getV()), dim(v.getDim()) {
	}

	int getDim() const {
		return dim;
	}

	int getV() const {
		return v;
	}

	int getX() const {
		return v % dim;
	}

	int getY() const {
		return v / dim;
	}
};

std::string v2t(const Vertex & v)
{
	char xc = 'A' + v.getX();
	if (xc >= 'I')
		xc++;

	return myformat("%c%c", xc, '1' + v.getY());

}

auto vertexCmp = [](const Vertex & a, const Vertex & b)
{
	return a.getV() > b.getV();
};

typedef struct {
	std::set<Vertex, decltype(vertexCmp)> empty;
	bool valid;
} empty_vertexes_t;

typedef struct {
	player_t color;
	std::set<Vertex, decltype(vertexCmp)> chain;
	std::set<Vertex, decltype(vertexCmp)> freedoms;
} chain_t;

void dump(const std::vector<empty_vertexes_t> & empties)
{
	printf("Valid empty crosses:\n");
	for(auto ev : empties) {
		assert(ev.valid);

		printf(" - ");

		for(auto v : ev.empty)
			printf("%s ", v2t(v).c_str());

		printf("\n");
	}
}

void dump(const chain_t & chain)
{
	printf("Chain for %s:\n", chain.color == P_BLACK ? "black" : "white");

	for(auto v : chain.chain) 
		printf("%s ", v2t(v).c_str());
	printf("\n");

	printf("Freedoms of that chain:\n");

	for(auto v : chain.freedoms) 
		printf("%s ", v2t(v).c_str());
	printf("\n");
}

void dump(const std::vector<chain_t> & chains)
{
	for(auto chain : chains)
		dump(chain);
}

class Board {
private:
	const int dim;
	board_t *const b;

public:
	Board(const int dim) : dim(dim), b(new board_t[dim * dim]) {
		assert(dim & 1);
	}

	~Board() {
		delete [] b;
	}

	Board(const Board & bIn) : dim(bIn.getDim()), b(new board_t[dim * dim]) {
		assert(dim & 1);

		for(int i=0; i<dim*dim; i++)
			b[i] = bIn.getAt(i);
	}

	int getDim() const {
		return dim;
	}

	board_t getAt(const int v) const {
		assert(v < dim * dim);
		assert(v >= 0);
		return b[v];
	}

	board_t getAt(const int x, const int y) const {
		assert(x < dim && x >= 0);
		assert(y < dim && y >= 0);
		int v = y * dim + x;
		return b[v];
	}

	void setAt(const int v, const board_t bv) {
		assert(v < dim * dim);
		assert(v >= 0);
		b[v] = bv;
	}

	void setAt(const int x, const int y, const board_t bv) {
		assert(x < dim && x >= 0);
		assert(y < dim && y >= 0);
		int v = y * dim + x;
		b[v] = bv;
	}
};

Board stringToBoard(const std::string & in)
{
	const int dim = in.find("\n");
	Board b(dim);

	int v = 0;

	for(auto c : in) {
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

void dump(const Board & b)
{
	const int dim = b.getDim();

	for(int y=dim - 1; y>=0; y--) {
		printf("%2d | ", y + 1);
		for(int x=0; x<dim; x++) {
			board_t bv = b.getAt(x, y);

			if (bv == B_EMPTY)
				printf(".");
			else if (bv == B_BLACK)
				printf("x");
			else if (bv == B_WHITE)
				printf("o");
			else
				printf("!");
		}

		printf("\n");
	}

	printf("     ");

	for(int x=0; x<dim; x++) {
		int xc = 'A' + x;

		if (xc >= 'I')
			xc++;

		printf("%c", xc);
	}

	printf("\n");
}

void scanChains(Board *const b, chain_t *const curChain, const int x, const int y, const board_t & startType)
{
	board_t bv = b->getAt(x, y);

	const int dim = b->getDim();

	if (bv == startType) {
		b->setAt(x, y, B_IGNORE);

		curChain->chain.insert(Vertex(x, y, dim));

		if (y > 0)
			scanChains(b, curChain, x, y - 1, startType);
		if (y < dim - 1)
			scanChains(b, curChain, x, y + 1, startType);
		if (x > 0)
			scanChains(b, curChain, x - 1, y, startType);
		if (x < dim - 1)
			scanChains(b, curChain, x + 1, y, startType);
	}
	else if (bv == B_EMPTY) {
		curChain->freedoms.insert(Vertex(x, y, dim));
	}
}

void findChains(const Board & b, std::vector<chain_t> *const chainsWhite, std::vector<chain_t> *const chainsBlack)
{
	Board work(b);

	const int dim = work.getDim();

	for(int y=0; y<dim; y++) {
		for(int x=0; x<dim; x++) {
			board_t bv = work.getAt(x, y);

			if (bv != B_BLACK && bv != B_WHITE)
				continue;

			chain_t curChain;
			curChain.color = bv == B_BLACK ? P_BLACK : P_WHITE;

			scanChains(&work, &curChain, x, y, bv);

			if (curChain.color == P_WHITE)
				chainsWhite->push_back(curChain);
			else
				chainsBlack->push_back(curChain);
		}
	}
}

void scanEmpty(Board *const b, const int x, const int y, const board_t & myStone, std::vector<chain_t> & chains, empty_vertexes_t *const empty)
{
	bool stop = false;

	const int dim = b->getDim();

	if (x < 0 || y < 0 || x >= dim || y >= dim) {
		empty->valid = true;
		stop = true;
	}
	else if (b->getAt(x, y) == B_EMPTY) {
		b->setAt(x, y, B_IGNORE);

		empty->empty.insert(Vertex(x, y, dim));
	}
	else if (b->getAt(x, y) == myStone && empty->valid == false) {
		stop = true;

		for(auto c : chains) {
			assert(c.color == (myStone == B_BLACK ? P_BLACK : P_WHITE));

			Vertex v(x, y, dim);

			if (c.freedoms.size() > 1 && c.chain.find(v) != c.chain.end()) {
				empty->valid = true;
				break;
			}
		}
	}
	else {
		stop = true;
	}

	if (!stop) {
		scanEmpty(b, x, y - 1, myStone, chains, empty);
		scanEmpty(b, x, y + 1, myStone, chains, empty);
		scanEmpty(b, x - 1, y, myStone, chains, empty);
		scanEmpty(b, x + 1, y, myStone, chains, empty);
	}
}

std::vector<empty_vertexes_t> findValidEmptyVertexes(const Board & b, const player_t & p, std::vector<chain_t> & chains)
{
	std::vector<empty_vertexes_t> out;

	Board work(b);

	board_t myStone = p == P_BLACK ? B_BLACK : B_WHITE;

	const int dim = work.getDim();

	for(int y=0; y<dim; y++) {
		for(int x=0; x<dim; x++) {
			const int v = y * dim + x;

			board_t bv = work.getAt(v);

			if (bv == B_EMPTY) {
				empty_vertexes_t empty;
				empty.valid = false;

				scanEmpty(&work, x, y, myStone, chains, &empty);

				if (empty.valid && empty.empty.empty() == false)
					out.push_back(empty);
			}
		}
	}

	return out;
}

int main(int argc, char *argv[])
{
	Board b = stringToBoard(
			".........\n"
			".........\n"
			".........\n"
			".........\n"
			".........\n"
			".xxxx....\n"
			".x..x....\n"
			".xxxx....\n"
			".........\n"
			);


	dump(b);

	std::vector<chain_t> chainsWhite, chainsBlack;
	findChains(b, &chainsWhite, &chainsBlack);
	dump(chainsWhite);
	dump(chainsBlack);

	std::vector<empty_vertexes_t> evs = findValidEmptyVertexes(b, P_WHITE, chainsBlack);
	dump(evs);

	return 0;
}
