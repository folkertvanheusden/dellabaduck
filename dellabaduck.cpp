#include <assert.h>
#include <ctype.h>
#include <fstream>
#include <optional>
#include <set>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <vector>

#include "str.h"

typedef enum { P_BLACK, P_WHITE } player_t;
typedef enum { B_EMPTY, B_WHITE, B_BLACK, B_IGNORE } board_t;
const char *const board_t_names[] = { "empty", "white", "black", "ignore" };

FILE *fh = fopen("/tmp/input.dat", "a+");

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

	return Vertex(x, y, dim);
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
	board_t type;
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
	printf("Chain for %s:\n", board_t_names[chain.type]);

	for(auto v : chain.chain) 
		printf("%s ", v2t(v).c_str());
	printf("\n");

	if (chain.freedoms.empty() == false) {
		printf("Freedoms of that chain:\n");

		for(auto v : chain.freedoms) 
			printf("%s ", v2t(v).c_str());
		printf("\n");
	}
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

	void setAt(const Vertex & v, const board_t bv) {
		b[v.getV()] = bv;
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
	else if (bv == B_EMPTY || bv == B_IGNORE) {
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

			if (bv == B_IGNORE)
				continue;

			chain_t curChain;
			curChain.type = bv;

			scanChains(&work, &curChain, x, y, bv);

			if (curChain.type == B_WHITE)
				chainsWhite->push_back(curChain);
			else if (curChain.type == B_BLACK)
				chainsBlack->push_back(curChain);
		}
	}
}

bool connectedToChain(const Vertex & v, const chain_t & chain)
{
	const int x = v.getX();
	const int y = v.getY();

	for(auto stone : chain.chain) {
		int stone_x = stone.getX();
		int stone_y = stone.getY();

		if (((stone_x == x - 1 || stone_x == x + 1) && stone_y == y) ||
		    ((stone_y == y - 1 || stone_y == y + 1) && stone_x == x))
			return true;
	}

	return false;
}

bool connectedToChain(const chain_t & chain1, const chain_t & chain2)
{
	for(auto stone : chain1.chain) {
		if (connectedToChain(stone, chain2))
			return true;
	}

	return false;
}

void purgeFreedoms(std::vector<chain_t> *const chainsPurge, const std::vector<chain_t> & chainsRef)
{
	for(auto it = chainsPurge->begin(); it != chainsPurge->end(); it++) {
		if (it->freedoms.size() > 1)
			continue;

		bool willPurge = false;
		for(auto chainRef : chainsRef) {
			if (chainRef.freedoms.size() > 1)
				continue;

			if (connectedToChain(*it, chainRef)) {
				willPurge = true;
				break;
			}
		}

		if (willPurge) {
			dump(*it);
			it = chainsPurge->erase(it);
		}
	}
}

void play(Board *const b, const Vertex & v, const player_t & p)
{
	b->setAt(v, p == P_BLACK ? B_BLACK : B_WHITE);

	std::vector<chain_t> chainsWhite, chainsBlack;
	findChains(*b, &chainsWhite, &chainsBlack);

	std::vector<chain_t> & scan = p == P_BLACK ? chainsWhite : chainsBlack;

	for(auto chain : scan) {
		if (chain.freedoms.empty()) {
			for(auto v : chain.chain)
				b->setAt(v, B_EMPTY);
		}
	}
}

std::optional<Vertex> genMove(const Board & b, const player_t & p)
{
	std::vector<chain_t> chainsWhite, chainsBlack;
	findChains(b, &chainsWhite, &chainsBlack);

#if 0
	std::vector<empty_vertexes_t> evs = findValidEmptyVertexes(b, p, chainsBlack, chainsWhite);

	if (evs.empty())
		return { };

	return *evs.at(0).empty.begin();
#endif
	return { };
}

int main(int argc, char *argv[])
{
#if 1
        Board b = stringToBoard(
                        "...o.\n"
                        "oo.oo\n"
                        "..o..\n"
                        "oo.oo\n"
                        "..o.x\n"
                        );


        dump(b);

        std::vector<chain_t> chainsWhite, chainsBlack;
        findChains(b, &chainsWhite, &chainsBlack);
        dump(chainsWhite);
 //       dump(chainsBlack);

	return 0;
	printf("purge white\n");
	purgeFreedoms(&chainsWhite, chainsBlack);
	printf("purge black\n");
	purgeFreedoms(&chainsBlack, chainsWhite);
#else
	Board *b = new Board(9);

	setbuf(stdout, NULL);
	setbuf(stderr, NULL);

	for(;;) {
		char buffer[4096] { 0 };
		if (!fgets(buffer, sizeof buffer, stdin))
			break;

		char *lf = strchr(buffer, '\n');
		if (lf)
			*lf = 0x00;

		if (buffer[0] == 0x00)
			continue;

		fprintf(fh, "> %s\n", buffer);

		std::vector<std::string> parts = split(buffer, " ");

		std::string id;
		if (isdigit(parts.at(0).at(0))) {
			id = parts.at(0);
			parts.erase(parts.begin() + 0);
		}

		if (parts.at(0) == "protocol_version")
			printf("=%s 2\n\n", id.c_str());
		else if (parts.at(0) == "name")
			printf("=%s DellaBaduck\n\n", id.c_str());
		else if (parts.at(0) == "version")
			printf("=%s 0.1\n\n", id.c_str());
		else if (parts.at(0) == "boardsize") {
			delete b;
			b = new Board(atoi(parts.at(1).c_str()));
			printf("=%s\n\n", id.c_str());
		}
		else if (parts.at(0) == "clear_board") {
			int dim = b->getDim();
			delete b;
			b = new Board(dim);
			printf("=%s\n\n", id.c_str());
		}
		else if (parts.at(0) == "play") {
			Vertex v = t2v(parts.at(2), b->getDim());
			play(b, v, (parts.at(1) == "b" || parts.at(1) == "black") ? P_BLACK : P_WHITE);
			printf("=%s\n\n", id.c_str());
		}
		else if (parts.at(0) == "dump") {
			dump(*b);
		}
		else if (parts.at(0) == "quit")
			break;
		else if (parts.at(0) == "known_command") {  // TODO
			if (parts.at(1) == "known_command")
				printf("=%s true\n\n", id.c_str());
			else
				printf("=%s false\n\n", id.c_str());
		}
		else if (parts.at(0) == "komi") {
			printf("=%s\n\n", id.c_str());  // TODO
		}
		else if (parts.at(0) == "time_settings") {
			printf("=%s\n\n", id.c_str());  // TODO
		}
		else if (parts.at(0) == "time_left") {
			printf("=%s\n\n", id.c_str());  // TODO
		}
		else if (parts.at(0) == "list_commands") {
			printf("=%s name\n", id.c_str());
			printf("=%s version\n", id.c_str());
			printf("=%s boardsize\n", id.c_str());
			printf("=%s clear_board\n", id.c_str());
			printf("=%s play\n", id.c_str());
			printf("=%s genmove\n", id.c_str());
			printf("\n");
		}
		else if (parts.at(0) == "genmove") {
			player_t player = (parts.at(1) == "b" || parts.at(1) == "black") ? P_BLACK : P_WHITE;
			auto v = genMove(*b, player);

			if (v.has_value()) {
				printf("=%s %s\n\n", id.c_str(), v2t(v.value()).c_str());
				fprintf(fh, "< %s\n", v2t(v.value()).c_str());

				play(b, v.value(), player);
			}
			else {
				printf("=%s pass\n\n", id.c_str());
			}
		}
		else {
			printf("?\n\n");
		}

		fflush(nullptr);
	}
#endif
	fclose(fh);

	return 0;
}
