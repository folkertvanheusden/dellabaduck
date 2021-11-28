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
#include "time.h"

typedef enum { P_BLACK, P_WHITE } player_t;
typedef enum { B_EMPTY, B_WHITE, B_BLACK } board_t;
const char *const board_t_names[] = { "empty", "white", "black" };

FILE *fh = fopen("/tmp/input.dat", "a+");

class Vertex
{
private:
	const int v, dim;

public:
	Vertex(const int v, const int dim) : v(v), dim(dim) {
	}

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
	board_t type;
	std::set<Vertex, decltype(vertexCmp)> chain;
	std::set<Vertex, decltype(vertexCmp)> freedoms;
} chain_t;

void dump(const chain_t & chain)
{
	fprintf(fh, "# Chain for %s:\n", board_t_names[chain.type]);

	fprintf(fh, "# ");
	for(auto v : chain.chain) 
		fprintf(fh, "%s ", v2t(v).c_str());
	fprintf(fh, "\n");

	if (chain.freedoms.empty() == false) {
		fprintf(fh, "# Freedoms of that chain:\n");

		fprintf(fh, "# ");
		for(auto v : chain.freedoms) 
			fprintf(fh, "%s ", v2t(v).c_str());
		fprintf(fh, "\n");
	}
}

void dump(const std::vector<chain_t *> & chains)
{
	for(auto chain : chains)
		dump(*chain);
}

class Board {
private:
	const int dim;
	board_t *const b;

public:
	Board(const int dim) : dim(dim), b(new board_t[dim * dim]()) {
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
	auto templines = split(in, "\n");

	for(size_t i=0; i<templines.size() / 2; i++) {
		std::string temp = templines.at(i);
		templines.at(i) = templines.at(templines.size() - i - 1);
		templines.at(templines.size() - i - 1) = temp;
	}

	auto work = merge(templines, "\n");

	const int dim = work.find("\n");
	Board b(dim);

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

void dump(const Board & b)
{
	const int dim = b.getDim();

	for(int y=dim - 1; y>=0; y--) {
		fprintf(fh, "# %2d | ", y + 1);
		for(int x=0; x<dim; x++) {
			board_t bv = b.getAt(x, y);

			if (bv == B_EMPTY)
				fprintf(fh, ".");
			else if (bv == B_BLACK)
				fprintf(fh, "x");
			else if (bv == B_WHITE)
				fprintf(fh, "o");
			else
				fprintf(fh, "!");
		}

		fprintf(fh, "\n");
	}

	fprintf(fh, "#      ");

	for(int x=0; x<dim; x++) {
		int xc = 'A' + x;

		if (xc >= 'I')
			xc++;

		fprintf(fh, "%c", xc);
	}

	fprintf(fh, "\n");
}

class ChainMap {
private:
	const int dim;
	chain_t **const cm;

public:
	ChainMap(const int dim) : dim(dim), cm(new chain_t *[(dim + 2) * (dim + 2)]()) {
		assert(dim & 1);
	}

	~ChainMap() {
		delete [] cm;
	}

	int getDim() const {
		return dim;
	}

	chain_t * getAt(const int x, const int y) {
		assert(x < dim + 1 && x >= -1);
		assert(y < dim + 1 && y >= -1);
		int v = (y + 1) * dim + x + 1;
		return cm[v];
	}

	void setAt(const Vertex & v, chain_t *const chain) {
		setAt(v.getX(), v.getY(), chain);
	}

	void setAt(const int x, const int y, chain_t *const chain) {
		assert(x < dim + 1 && x >= -1);
		assert(y < dim + 1 && y >= -1);
		int v = (y + 1) * dim + x + 1;
		cm[v] = chain;
	}
};

void scanChains(const Board & b, bool *const scanned, chain_t *const curChain, const int x, const int y, const board_t & startType, ChainMap *const cm)
{
	board_t bv = b.getAt(x, y);

	const int dim = b.getDim();

	const int v = y * dim + x;

	if (bv == startType && scanned[v] == false) {
		scanned[v] = true;

		cm->setAt(x, y, curChain);

		curChain->chain.insert(Vertex(x, y, dim));

		if (y > 0)
			scanChains(b, scanned, curChain, x, y - 1, startType, cm);
		if (y < dim - 1)
			scanChains(b, scanned, curChain, x, y + 1, startType, cm);
		if (x > 0)
			scanChains(b, scanned, curChain, x - 1, y, startType, cm);
		if (x < dim - 1)
			scanChains(b, scanned, curChain, x + 1, y, startType, cm);
	}
	else if (bv == B_EMPTY) {
		curChain->freedoms.insert(Vertex(x, y, dim));  // only for counting the total number of freedoms
	}
}

void findChains(const Board & b, std::vector<chain_t *> *const chainsWhite, std::vector<chain_t *> *const chainsBlack, ChainMap *const cm)
{
	const int dim = b.getDim();

	bool *scanned = new bool[dim * dim]();

	for(int y=0; y<dim; y++) {
		for(int x=0; x<dim; x++) {
			if (scanned[y * dim + x])
				continue;

			board_t bv = b.getAt(x, y);
			if (bv == B_EMPTY)
				continue;

			chain_t *curChain = new chain_t;
			curChain->type = bv;

			scanChains(b, scanned, curChain, x, y, bv, cm);

			if (curChain->type == B_WHITE)
				chainsWhite->push_back(curChain);
			else if (curChain->type == B_BLACK)
				chainsBlack->push_back(curChain);
		}
	}

	delete [] scanned;
}

void scanChainsOfFreedoms(const Board & b, bool *const scanned, chain_t *const curChain, const int x, const int y)
{
	board_t bv = b.getAt(x, y);

	const int dim = b.getDim();

	const int v = y * dim + x;

	if (bv == B_EMPTY && scanned[v] == false) {
		scanned[v] = true;

		curChain->chain.insert(Vertex(x, y, dim));

		if (y > 0)
			scanChainsOfFreedoms(b, scanned, curChain, x, y - 1);
		if (y < dim - 1)
			scanChainsOfFreedoms(b, scanned, curChain, x, y + 1);
		if (x > 0)
			scanChainsOfFreedoms(b, scanned, curChain, x - 1, y);
		if (x < dim - 1)
			scanChainsOfFreedoms(b, scanned, curChain, x + 1, y);
	}
}

void findChainsOfFreedoms(const Board & b, std::vector<chain_t *> *const chainsEmpty)
{
	const int dim = b.getDim();

	bool *scanned = new bool[dim * dim]();

	for(int y=0; y<dim; y++) {
		for(int x=0; x<dim; x++) {
			if (scanned[y * dim + x])
				continue;

			board_t bv = b.getAt(x, y);
			if (bv != B_EMPTY)
				continue;

			chain_t *curChain = new chain_t;
			curChain->type = B_EMPTY;

			scanChainsOfFreedoms(b, scanned, curChain, x, y);

			chainsEmpty->push_back(curChain);
		}
	}

	delete [] scanned;
}

void purgeChains(std::vector<chain_t *> *const chains)
{
	for(auto chain : *chains)
		delete chain;
}

void purgeFreedoms(std::vector<chain_t *> *const chainsPurge, ChainMap & cm, const board_t me)
{
	// go through all chains from chainsPurge
	for(auto it = chainsPurge->begin(); it != chainsPurge->end();) {
		bool considerPurge = false;

		bool mustPurge = (*it)->chain.size() == 1;

		if (!mustPurge) {
			// - if a freedom is 'connected to an opponent chain' with 1
			//   freedom, then valid
			// - if a freedom is 'connected to a chain of myself' with 1
			//   freedom, then invalid
			for(auto stone : (*it)->chain) {
				const int x = stone.getX();
				const int y = stone.getY();

				const chain_t *const north = cm.getAt(x, y - 1);
				if (north && north->type == me && north->freedoms.size() == 1)
					considerPurge = true;

				const chain_t *const west  = cm.getAt(x - 1, y);
				if (west && west->type == me && west->freedoms.size() == 1)
					considerPurge = true;

				const chain_t *const south = cm.getAt(x, y + 1);
				if (south && south->type == me && south->freedoms.size() == 1)
					considerPurge = true;

				const chain_t *const east  = cm.getAt(x + 1, y);
				if (east && east->type == me && east->freedoms.size() == 1)
					considerPurge = true;
			}
		}

		if ((considerPurge && (*it)->chain.size() == 1) || mustPurge) {
			delete *it;
			it = chainsPurge->erase(it);
		}
		else {
			it++;
		}
	}
}

void play(Board *const b, const Vertex & v, const player_t & p)
{
	b->setAt(v, p == P_BLACK ? B_BLACK : B_WHITE);

	ChainMap cm(b->getDim());
	std::vector<chain_t *> chainsWhite, chainsBlack;
	findChains(*b, &chainsWhite, &chainsBlack, &cm);

	std::vector<chain_t *> & scan = p == P_BLACK ? chainsWhite : chainsBlack;

	for(auto chain : scan) {
		if (chain->freedoms.empty()) {
			for(auto v : chain->chain)
				b->setAt(v, B_EMPTY);
		}
	}

	purgeChains(&chainsBlack);
	purgeChains(&chainsWhite);
}

typedef struct {
        int score;
        bool valid;
} eval_t;

bool isValidMove(const std::vector<chain_t *> & chainsEmpty, const Vertex & v)
{
	for(auto chain : chainsEmpty) {
		if (chain->chain.find(v) != chain->chain.end())
			return true;
	}

	return false;
}

void selectRandom(const std::vector<chain_t *> & chainsWhite, const std::vector<chain_t *> & chainsBlack, const std::vector<chain_t *> & chainsEmpty, std::vector<eval_t> *const evals)
{
	auto chain = chainsEmpty.at(rand() % chainsEmpty.size());
	size_t chainSize = chain->chain.size();
	int r = rand() % (chainSize - 1);

	auto it = chain->chain.begin();
	for(int i=0; i<r; i++)
		it++;

	const int v = it->getV();

	evals->at(v).score++;
	evals->at(v).valid = true;
}

std::optional<Vertex> genMove(const Board & b, const player_t & p)
{
	const int dim = b.getDim();

	// find chains of stones
	ChainMap cm(dim);
	std::vector<chain_t *> chainsWhite, chainsBlack;
	findChains(b, &chainsWhite, &chainsBlack, &cm);

	// find chains of freedoms
	std::vector<chain_t *> chainsEmpty;
	findChainsOfFreedoms(b, &chainsEmpty);
	purgeFreedoms(&chainsEmpty, cm, p == P_BLACK ? B_BLACK : B_WHITE);

	// no valid freedoms? return "pass".
	if (chainsEmpty.empty())
		return { };

        std::vector<eval_t> evals;
        for(int i=0; i<dim * dim; i++)
                evals.push_back({ 0, false });

	// algorithms
	selectRandom(chainsWhite, chainsBlack, chainsEmpty, &evals);

	// find best
	std::optional<Vertex> v;

        int bestScore = -32767, bestSq = -1;
        for(int i=0; i<dim * dim; i++) {
                if (evals.at(i).score > bestScore && evals.at(i).valid) {
                        Vertex temp = Vertex(i, dim);

			if (isValidMove(chainsEmpty, temp)) {
                                v.emplace(temp);
                                bestScore = evals.at(i).score;
                                bestSq = i;
                        }
                }
        }

	purgeChains(&chainsBlack);
	purgeChains(&chainsWhite);
	purgeChains(&chainsEmpty);

	return v;
}

void load_stones(Board *const b, const char *in, const board_t & bv)
{
	while(in[0] == '[') {
		const char *end = strchr(in, ']');

		char xc = tolower(in[1]);
		char yc = tolower(in[2]);

		int x = xc - 'a';
		int y = yc - 'a';

		b->setAt(x, y, bv);

		in = end + 1;
	}
}

Board loadSgf(const std::string & filename)
{
	FILE *sfh = fopen(filename.c_str(), "r");
	if (!sfh) {
		fprintf(fh, "Cannot open %s\n", filename.c_str());
		return Board(9);
	}

	char buffer[65536];
	fgets(buffer, sizeof buffer, sfh);

	int dim = 19;

	const char *SZ = strstr(buffer, "SZ[");
	if (SZ)
		dim = atoi(SZ + 3);

	Board b(dim);

	const char *AB = strstr(buffer, "AB[");
	if (AB)
		load_stones(&b, AB + 2, B_BLACK);

	const char *AW = strstr(buffer, "AW[");
	if (AW)
		load_stones(&b, AW, B_WHITE);

	fclose(sfh);

	return b;
}

int main(int argc, char *argv[])
{
#if 0
#if 0
        Board b = stringToBoard(
                        "...o.\n"
                        "oo.oo\n"
                        "..o..\n"
                        "oo.oo\n"
                        "..o.x\n"
                        );
#else
        Board b = stringToBoard(
                        "o....\n"
                        ".o.oo\n"
                        "o.oo.\n"
                        ".o.o.\n"
                        "o.o.o\n"
                        );
#endif

#if 0
        dump(b);

	ChainMap cm(b.getDim());
        std::vector<chain_t *> chainsWhite, chainsBlack;
        findChains(b, &chainsWhite, &chainsBlack, &cm);
        //dump(chainsWhite);
        dump(chainsBlack);

	std::vector<chain_t *> chainsEmpty;
	findChainsOfFreedoms(b, &chainsEmpty);
	fprintf(fh, "\n\npurge empty\n");
	purgeFreedoms(&chainsEmpty, cm, B_BLACK);
//	dump(chainsEmpty);

	purgeChains(&chainsBlack);
	purgeChains(&chainsWhite);
	purgeChains(&chainsEmpty);
#endif
	dump(b);
	auto v = genMove(b, P_BLACK);
	if (v.has_value())
		fprintf(fh, "= %s\n\n", v2t(v.value()).c_str());
	else
		fprintf(fh, "pass\n");

	fclose(fh);

	return 0;
#elif 1
	Board *b = new Board(9);

	setbuf(stdout, nullptr);
	setbuf(stderr, nullptr);

	srand(time(nullptr));

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
			if (str_tolower(parts.at(2)) != "pass") {
				Vertex v = t2v(parts.at(2), b->getDim());
				play(b, v, (parts.at(1) == "b" || parts.at(1) == "black") ? P_BLACK : P_WHITE);
			}

			printf("=%s\n\n", id.c_str());
		}
		else if (parts.at(0) == "dump") {
			dump(*b);
		}
		else if (parts.at(0) == "quit") {
			printf("=%s\n\n", id.c_str());
			break;
		}
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
			printf("=%s komi\n", id.c_str());
			printf("=%s quit\n", id.c_str());
			printf("=%s loadsgf\n", id.c_str());
			printf("\n");
		}
		else if (parts.at(0) == "loadsgf") {
			delete b;
			b = new Board(loadSgf(parts.at(1)));

			printf("=%s\n\n", id.c_str());
		}
		else if (parts.at(0) == "genmove" || parts.at(0) == "reg_genmove") {
			player_t player = (parts.at(1) == "b" || parts.at(1) == "black") ? P_BLACK : P_WHITE;
			auto v = genMove(*b, player);

			if (v.has_value()) {
				printf("=%s %s\n\n", id.c_str(), v2t(v.value()).c_str());
				fprintf(fh, "< %s\n", v2t(v.value()).c_str());

				if (parts.at(0) == "genmove")
					play(b, v.value(), player);
			}
			else {
				printf("=%s pass\n\n", id.c_str());
			}
		}
		else if (parts.at(0) == "final_score") {
			printf("=%s 0.0\n\n", id.c_str());  // TODO
		}
		else {
			printf("?\n\n");
		}

		fflush(nullptr);
	}

	delete b;
#elif 0
	// benchmark
	uint64_t start = get_ts_ms(), end = 0;
	uint64_t n = 0;

	do {
		Board b(9);

		player_t p = P_BLACK;

		int mc = 0;

		for(;;) {
			auto v = genMove(b, p);
			if (!v.has_value())
				break;

			play(b, v.value(), p);

			p = p == P_BLACK ? P_WHITE : P_BLACK;
			mc++;

			if (mc == 150)
				break;
		}

		n++;

		end = get_ts_ms();
	}
	while(end - start < 5000);

	printf("%f\n", n * 1000.0 / (end - start));
#endif
	fclose(fh);

	return 0;
}
