#include <assert.h>
#include <ctype.h>
#include <fstream>
#include <optional>
#include <set>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <vector>
#include <sys/resource.h>
#include <sys/time.h>

#include "str.h"
#include "time.h"

typedef enum { P_BLACK, P_WHITE } player_t;
typedef enum { B_EMPTY, B_WHITE, B_BLACK, B_LAST } board_t;
const char *const board_t_names[] = { ".", "o", "x" };

FILE *fh = fopen("/tmp/input.dat", "a+");

bool cgos = true;

void send(const bool tx, const char *fmt, ...)
{
	uint64_t now = get_ts_ms();
	time_t t_now = now / 1000;

	struct tm tm { 0 };
	if (!localtime_r(&t_now, &tm))
		fprintf(stderr, "localtime_r: %s\n", strerror(errno));

	char *ts_str = nullptr;
	asprintf(&ts_str, "%04d-%02d-%02d %02d:%02d:%02d.%03d ",
			tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec, int(now % 1000));

	char *str = nullptr;

	va_list ap;
	va_start(ap, fmt);
	(void)vasprintf(&str, fmt, ap);
	va_end(ap);

	fprintf(fh, "%s%s\n", ts_str, str);
	fflush(fh);

	if (tx) {
		if (cgos && fmt[0] == '#')
			return;

		printf("%s\n", str);
	}

	free(str);
	free(ts_str);
}

board_t playerToStone(const player_t & p)
{
	return p == P_BLACK ? B_BLACK : B_WHITE;
}

player_t getOpponent(const player_t & p)
{
	return p == P_WHITE ? P_BLACK : P_WHITE;
}

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

	return { x, y, dim };
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
	send(true, "# Chain for %s:", board_t_names[chain.type]);

	std::string line = "# ";
	for(auto v : chain.chain) 
		line += myformat("%s ", v2t(v).c_str());
	send(true, line.c_str());

	if (chain.freedoms.empty() == false) {
		send(true, "# Freedoms of that chain:");

		line = "# ";
		for(auto v : chain.freedoms) 
			line += myformat("%s ", v2t(v).c_str());
		send(true, line.c_str());
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

		bIn.getTo(b);
	}

	int getDim() const {
		return dim;
	}

	void getTo(board_t *const bto) const {
		memcpy(bto, b, dim * dim * sizeof(*b));
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

		send(true, line.c_str());
	}

	line = "#      ";

	for(int x=0; x<dim; x++) {
		int xc = 'A' + x;

		if (xc >= 'I')
			xc++;

		line += myformat("%c", xc);
	}

	send(true, line.c_str());
}

class ChainMap {
private:
	const int dim;
	chain_t **const cm;

public:
	ChainMap(const int dim) : dim(dim), cm(new chain_t *[dim * dim]()) {
		assert(dim & 1);
	}

	~ChainMap() {
		delete [] cm;
	}

	int getDim() const {
		return dim;
	}

	chain_t * getAt(const int x, const int y) const {
		assert(x < dim && x >= 0);
		assert(y < dim && y >= 0);
		int v = y * dim + x;
		return cm[v];
	}

	void setAt(const Vertex & v, chain_t *const chain) {
		setAt(v.getX(), v.getY(), chain);
	}

	void setAt(const int x, const int y, chain_t *const chain) {
		assert(x < dim && x >= 0);
		assert(y < dim && y >= 0);
		int v = y * dim + x;
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

		curChain->chain.insert({ x, y, dim });

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
		curChain->freedoms.insert({ x, y, dim });  // only for counting the total number of freedoms
	}
}

void findChains(const Board & b, std::vector<chain_t *> *const chainsWhite, std::vector<chain_t *> *const chainsBlack, ChainMap *const cm)
{
	const int dim = b.getDim();

	bool *scanned = new bool[dim * dim]();

	for(int y=0; y<dim; y++) {
		for(int x=0; x<dim; x++) {
			int v = y * dim + x;

			if (scanned[v])
				continue;

			board_t bv = b.getAt(v);
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

		curChain->chain.insert({ x, y, dim });

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
			int v = y * dim + x;

			if (scanned[v])
				continue;

			board_t bv = b.getAt(v);
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

void purgeFreedoms(std::vector<chain_t *> *const chainsPurge, const ChainMap & cm, const board_t me)
{
	const int dim = cm.getDim();

	// go through all chains from chainsPurge
	for(auto it = chainsPurge->begin(); it != chainsPurge->end();) {
		bool mustPurge = (*it)->chain.size() == 1;

		if (mustPurge) {
			delete *it;
			it = chainsPurge->erase(it);
		}
		else {
			it++;
		}
	}
}

void play(Board *const b, const Vertex & v, const player_t & p, bool debug)
{
	if (debug)
	send(false, "# set at %s", v2t(v).c_str());

	b->setAt(v, playerToStone(p));

	ChainMap cm(b->getDim());
	std::vector<chain_t *> chainsWhite, chainsBlack;
	findChains(*b, &chainsWhite, &chainsBlack, &cm);

	std::vector<chain_t *> & scan = p == P_BLACK ? chainsWhite : chainsBlack;

	for(auto chain : scan) {
		if (chain->freedoms.empty()) {
			for(auto ve : chain->chain) {
				if (debug)
				send(false, "# purge %s (no freedoms)", v2t(ve).c_str());
				b->setAt(ve, B_EMPTY);
			}
		}
	}

	purgeChains(&chainsBlack);
	purgeChains(&chainsWhite);
}

// black, white
std::pair<int, int> score(const Board & b)
{
	const int dim = b.getDim();

	// find chains of stones
	ChainMap cm(dim);
	std::vector<chain_t *> chainsWhite, chainsBlack;
	findChains(b, &chainsWhite, &chainsBlack, &cm);

	// number of stones for each
	int blackStones = 0;
	for(auto chain : chainsBlack)
		blackStones += chain->chain.size();

	int whiteStones = 0;
	for(auto chain : chainsWhite)
		whiteStones += chain->chain.size();

	int blackEmpty = 0, whiteEmpty = 0;
	for(int y=0; y<dim; y++) {
		for(int x=0; x<dim; x++) {
			if (b.getAt(x, y) != B_EMPTY)
				continue;

			bool has_b[B_LAST] { false };
			int xc = x, yc = y;
			while(xc >= 0 && b.getAt(xc, yc) == B_EMPTY)
				xc--;
			if (xc >= 0)
				has_b[b.getAt(xc, yc)] = true;

			xc = x, yc = y;
			while(xc < dim && b.getAt(xc, yc) == B_EMPTY)
				xc++;
			if (xc < dim)
				has_b[b.getAt(xc, yc)] = true;

			xc = x, yc = y;
			while(yc >= 0 && b.getAt(xc, yc) == B_EMPTY)
				yc--;
			if (yc >= 0)
				has_b[b.getAt(xc, yc)] = true;

			xc = x, yc = y;
			while(yc < dim && b.getAt(xc, yc) == B_EMPTY)
				yc++;
			if (yc < dim)
				has_b[b.getAt(xc, yc)] = true;

			int count_set = has_b[B_EMPTY] + has_b[B_WHITE] + has_b[B_BLACK];
			if (count_set < 3) {
				if (has_b[B_EMPTY] || count_set == 1) {
					if (has_b[B_WHITE])
						whiteEmpty++;
					else if (has_b[B_BLACK])
						blackEmpty++;
				}
			}
		}
	}

	purgeChains(&chainsBlack);
	purgeChains(&chainsWhite);

	int blackScore = blackStones + blackEmpty;
	int whiteScore = whiteStones + whiteEmpty;

	return { blackScore, whiteScore };
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

void selectRandom(const Board & b, const ChainMap & cm, const std::vector<chain_t *> & chainsWhite, const std::vector<chain_t *> & chainsBlack, const std::vector<chain_t *> & chainsEmpty, const player_t & p, std::vector<eval_t> *const evals)
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

std::vector<Vertex> pickEmptyAround(const ChainMap & cm, const Vertex & v)
{
	const int x = v.getX();
	const int y = v.getY();
	const int dim = cm.getDim();

	std::vector<Vertex> out;

	if (x > 0 && cm.getAt(x - 1, y) == nullptr)
		out.push_back({ x - 1, y });

	if (y > 0 && cm.getAt(x, y - 1) == nullptr)
		out.push_back({ x, y - 1 });

	if (x < dim - 1 && cm.getAt(x + 1, y) == nullptr)
		out.push_back({ x + 1, y });

	if (y < dim - 1 && cm.getAt(x, y + 1) == nullptr)
		out.push_back({ x, y + 1 });

	return out;
}

void selectExtendChains(const Board & b, const ChainMap & cm, const std::vector<chain_t *> & chainsWhite, const std::vector<chain_t *> & chainsBlack, const std::vector<chain_t *> & chainsEmpty, const player_t & p, std::vector<eval_t> *const evals)
{
	const std::vector<chain_t *> & scan = p == P_BLACK ? chainsWhite : chainsBlack;

	for(auto chain : scan) {
		for(auto stone : chain->chain) {
			auto empties = pickEmptyAround(cm, stone);

			for(auto stone : empties) {
				if (isValidMove(chainsEmpty, stone)) {
					int v = stone.getV();
					evals->at(v).score += 2;
					evals->at(v).valid = true;
				}
			}
		}
	}
}

void selectKillChains(const Board & b, const ChainMap & cm, const std::vector<chain_t *> & chainsWhite, const std::vector<chain_t *> & chainsBlack, const std::vector<chain_t *> & chainsEmpty, const player_t & p, std::vector<eval_t> *const evals)
{
	const std::vector<chain_t *> & scan = p == P_BLACK ? chainsBlack : chainsWhite;

	for(auto chain : scan) {
		const int add = chain->chain.size() - chain->freedoms.size();

		for(auto stone : chain->freedoms) {
			if (isValidMove(chainsEmpty, stone)) {
				int v = stone.getV();
				evals->at(v).score += add;
				evals->at(v).valid = true;
			}
		}
	}
}

void selectAtLeastOne(const Board & b, const ChainMap & cm, const std::vector<chain_t *> & chainsWhite, const std::vector<chain_t *> & chainsBlack, const std::vector<chain_t *> & chainsEmpty, const player_t & p, std::vector<eval_t> *const evals)
{
	auto chain = chainsEmpty.at(0);
	size_t chainSize = chain->chain.size();
	auto it = chain->chain.begin();
	const int v = it->getV();

	evals->at(v).score++;
	evals->at(v).valid = true;
}

int search(const Board & b, const player_t & p, int alpha, const int beta, const int depth)
{
	const int dim = b.getDim();

	std::vector<chain_t *> chainsEmpty;

	// do not calculate chains when at depth 0 as they're not used then
	if (depth > 0) {
		ChainMap cm(dim);
		std::vector<chain_t *> chainsWhite, chainsBlack;
		findChains(b, &chainsWhite, &chainsBlack, &cm);

		// find chains of freedoms
		findChainsOfFreedoms(b, &chainsEmpty);
		purgeFreedoms(&chainsEmpty, cm, playerToStone(p));

		purgeChains(&chainsBlack);
		purgeChains(&chainsWhite);
	}

	// no valid freedoms? return score (eval)
	if (chainsEmpty.empty()) {
		auto s = score(b);
		return p == P_BLACK ? s.first - s.second : s.second - s.first;
	}

	int bestScore = -32768;
	std::optional<Vertex> bestMove;

	player_t opponent = getOpponent(p);

	for(auto chain : chainsEmpty) {
		for(auto stone : chain->chain) {
			Board work(b);

			play(&work, stone, p, false);

			int score = -search(work, opponent, -beta, -alpha, depth - 1);

			if (score > bestScore) {
				bestScore = score;
				bestMove.emplace(stone);

				if (score > alpha) {
					alpha = score;

					if (score >= beta)
						goto finished;
				}
			}
		}
	}

finished:
	purgeChains(&chainsEmpty);

	return bestScore;
}

void selectAlphaBeta(const Board & b, const ChainMap & cm, const std::vector<chain_t *> & chainsWhite, const std::vector<chain_t *> & chainsBlack, const std::vector<chain_t *> & chainsEmpty, const player_t & p, std::vector<eval_t> *const evals, const double useTime)
{
	const int dim = b.getDim();

	bool *valid = new bool[dim * dim]();

	for(int i=0; i<dim * dim; i++)
		valid[i] = isValidMove(chainsEmpty, { i, dim });

	uint64_t start_t = get_ts_ms();  // TODO: start of genMove()
	uint64_t hend_t  = start_t + useTime * 1000 / 2;
	uint64_t end_t   = start_t + useTime * 1000;

	int depth = 1;

	while(get_ts_ms() < hend_t) {
		int *scores = new int[dim * dim]();

		send(false, "# a/b depth: %d", depth);

		bool to = false;

		for(int i=0; i<dim * dim; i++) {
			if (valid[i] == false)
				continue;

			if (get_ts_ms() >= end_t) {
				to = true;
				break;
			}

			Board work(b);

			play(&work, { i, dim }, p, false);

			scores[i] = search(work, p == P_BLACK ? P_WHITE : P_BLACK, -32768, 32768, depth);
		}

		if (!to) {
			for(int i=0; i<dim * dim; i++)
				evals->at(i).score += scores[i];
		}

		delete [] scores;

		depth++;
	}

	delete [] valid;
}

int calcN(const std::vector<chain_t *> & chains)
{
	int n = 0;

	for(auto chain : chains)
		n += chain->chain.size();

	return n;
}

std::optional<Vertex> genMove(Board *const b, const player_t & p, const bool doPlay, const double timeLeft)
{
	const int dim = b->getDim();
	const int p2dim = dim * dim;

	// find chains of stones
	ChainMap cm(dim);
	std::vector<chain_t *> chainsWhite, chainsBlack;
	findChains(*b, &chainsWhite, &chainsBlack, &cm);

	// find chains of freedoms
	std::vector<chain_t *> chainsEmpty;
	findChainsOfFreedoms(*b, &chainsEmpty);

	purgeFreedoms(&chainsEmpty, cm, playerToStone(p));

	// no valid freedoms? return "pass".
	if (chainsEmpty.empty()) {
		purgeChains(&chainsBlack);
		purgeChains(&chainsWhite);

		return { };
	}

	size_t totalNChains = chainsWhite.size() + chainsBlack.size();

	double useTime = (timeLeft / 2.) * totalNChains / p2dim * 0.95;

	send(false, "# timeLeft: %f, useTime: %f, total chain-count: %zu, board dimension: %d", timeLeft, useTime, totalNChains, dim);

        std::vector<eval_t> evals;
        for(int i=0; i<p2dim; i++)
                evals.push_back({ 0, false });

	// algorithms
// FIXME	selectRandom(*b, cm, chainsWhite, chainsBlack, chainsEmpty, p, &evals);

	selectExtendChains(*b, cm, chainsWhite, chainsBlack, chainsEmpty, p, &evals);

	selectKillChains(*b, cm, chainsWhite, chainsBlack, chainsEmpty, p, &evals);

	selectAtLeastOne(*b, cm, chainsWhite, chainsBlack, chainsEmpty, p, &evals);

	selectAlphaBeta(*b, cm, chainsWhite, chainsBlack, chainsEmpty, p, &evals, useTime);

	// find best
	std::optional<Vertex> v;

        int bestScore = -32767;
        for(int i=0; i<p2dim; i++) {
                if (evals.at(i).score > bestScore && evals.at(i).valid) {
                        Vertex temp { i, dim };

			if (isValidMove(chainsEmpty, temp)) {
                                v.emplace(temp);
                                bestScore = evals.at(i).score;
                        }
                }
        }

	// dump debug
	for(int y=dim-1; y>=0; y--) {
		std::string line = myformat("# %2d | ", y + 1);

		for(int x=0; x<dim; x++) {
			int v = y * dim + x;

			if (evals.at(v).valid)
				line += myformat("%3d ", evals.at(v).score);
			else
				line += myformat("  %s ", board_t_names[b->getAt(x, y)]);
		}

		send(true, line.c_str());
	}

	std::string line = "#      ";
	for(int x=0; x<dim; x++) {
		char c = 'A' + x;
		if (c >= 'I')
			c++;

		line += myformat(" %c  ", c);
	}

	send(true, line.c_str());

	// remove any chains that no longer have freedoms after this move
	// also play the move
	if (doPlay && v.has_value())
		play(b, v.value(), p, true);

	purgeChains(&chainsBlack);
	purgeChains(&chainsWhite);
	purgeChains(&chainsEmpty);

	return v;
}

double benchmark(const Board & in, const int ms)
{
	send(true, "# starting benchmark: duration: %.3fs, board dimensions: %d", ms / 1000.0, in.getDim());

	const int dim = in.getDim();

	// benchmark
	uint64_t start = get_ts_ms(), end = 0;
	uint64_t n = 0, total_puts = 0;

	do {
		Board b(in);

		player_t p = P_BLACK;

		int mc = 0;

		for(;;) {
			// find chains of stones
			ChainMap cm(in.getDim());
			std::vector<chain_t *> chainsWhite, chainsBlack;
			findChains(b, &chainsWhite, &chainsBlack, &cm);

			// find chains of freedoms
			std::vector<chain_t *> chainsEmpty;
			findChainsOfFreedoms(b, &chainsEmpty);
			purgeFreedoms(&chainsEmpty, cm, playerToStone(p));

			// no valid freedoms? return "pass".
			if (chainsEmpty.empty()) {
				purgeChains(&chainsBlack);
				purgeChains(&chainsWhite);
				break;
			}

			auto chain = chainsEmpty.at(rand() % chainsEmpty.size());
			size_t chainSize = chain->chain.size();
			int r = rand() % (chainSize - 1);

			auto it = chain->chain.begin();
			for(int i=0; i<r; i++)
				it++;

			b.setAt(*it, playerToStone(p));

			const int x = it->getX();
			const int y = it->getY();
			const board_t oppType = p == P_BLACK ? B_WHITE : B_BLACK;

			std::set<chain_t *> toPurge;
			if (y && b.getAt(x, y - 1) == oppType && cm.getAt(x, y - 1)->freedoms.size() == 1)
				toPurge.insert(cm.getAt(x, y - 1));
			if (y < dim - 1 && b.getAt(x, y + 1) == oppType && cm.getAt(x, y + 1)->freedoms.size() == 1)
				toPurge.insert(cm.getAt(x, y + 1));
			if (x && b.getAt(x - 1, y) == oppType && cm.getAt(x - 1, y)->freedoms.size() == 1)
				toPurge.insert(cm.getAt(x - 1, y));
			if (x < dim - 1 && b.getAt(x + 1, y) == oppType && cm.getAt(x + 1, y)->freedoms.size() == 1)
				toPurge.insert(cm.getAt(x + 1, y));

			for(auto chain : toPurge) {
				for(auto ve : chain->chain)
					b.setAt(ve, B_EMPTY);
			}

			purgeChains(&chainsEmpty);
			purgeChains(&chainsBlack);
			purgeChains(&chainsWhite);

			p = p == P_BLACK ? P_WHITE : P_BLACK;
			mc++;

			if (mc == 150)
				break;
		}

		total_puts += mc;

		n++;

		end = get_ts_ms();
	}
	while(end - start < ms);

	double pops = n * double(1000.0) / (end - start);
	send(true, "# playouts (%d) per second: %f (%.1f stones on average)", n, pops, total_puts / double(n));

	return pops;
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
		send(true, "Cannot open %s\n", filename.c_str());
		return Board(9);
	}

	char buffer[65536];
	fread(buffer, 1, sizeof buffer, sfh);

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
		load_stones(&b, AW + 2, B_WHITE);

	fclose(sfh);

	dump(b);

	return b;
}

int main(int argc, char *argv[])
{
#if 0
        Board b = stringToBoard(
                        ".o.o.\n"
                        "o.o.o\n"
                        ".oooo\n"
                        "ooooo\n"
                        "oo.o.\n"
                        );

	auto scores = score(b);

	std::string id;
	if (scores.first == scores.second)
		printf("=%s 0\n\n", id.c_str());
	else if (scores.first > scores.second)
		printf("=%s B+%d\n\n", id.c_str(), scores.first - scores.second);
	else
		printf("=%s W+%d\n\n", id.c_str(), scores.second - scores.first);

	return 0;

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
	auto v = genMove(b, P_BLACK, true);
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

	double timeLeft = -1;

	for(;;) {
		char buffer[4096] { 0 };
		if (!fgets(buffer, sizeof buffer, stdin))
			break;

		char *lf = strchr(buffer, '\n');
		if (lf)
			*lf = 0x00;

		if (buffer[0] == 0x00)
			continue;

		send(false, "> %s", buffer);

		std::vector<std::string> parts = split(buffer, " ");

		std::string id;
		if (isdigit(parts.at(0).at(0))) {
			id = parts.at(0);
			parts.erase(parts.begin() + 0);
		}

		if (parts.at(0) == "protocol_version")
			send(true, "=%s 2", id.c_str());
		else if (parts.at(0) == "name")
			send(true, "=%s DellaBaduck", id.c_str());
		else if (parts.at(0) == "version")
			send(true, "=%s 0.1", id.c_str());
		else if (parts.at(0) == "boardsize") {
			delete b;
			b = new Board(atoi(parts.at(1).c_str()));
			send(true, "=%s", id.c_str());
		}
		else if (parts.at(0) == "clear_board") {
			int dim = b->getDim();
			delete b;
			b = new Board(dim);
			send(true, "=%s", id.c_str());
		}
		else if (parts.at(0) == "play") {
			if (str_tolower(parts.at(2)) != "pass") {
				Vertex v = t2v(parts.at(2), b->getDim());
				play(b, v, (parts.at(1) == "b" || parts.at(1) == "black") ? P_BLACK : P_WHITE, false);
			}

			send(true, "=%s", id.c_str());
		}
		else if (parts.at(0) == "showboard") {
			dump(*b);
		}
		else if (parts.at(0) == "quit") {
			send(true, "=%s", id.c_str());
			break;
		}
		else if (parts.at(0) == "known_command") {  // TODO
			if (parts.at(1) == "known_command")
				send(true, "=%s true", id.c_str());
			else
				send(true, "=%s false", id.c_str());
		}
		else if (parts.at(0) == "benchmark") {
			// play outs per second
			double pops = benchmark(*b, parts.size() == 2 ? atoi(parts.at(1).c_str()) : 1000);

			send(true, "=%s %f", id.c_str(), pops);
		}
		else if (parts.at(0) == "komi") {
			send(true, "=%s", id.c_str());  // TODO
		}
		else if (parts.at(0) == "time_settings") {
			send(true, "=%s", id.c_str());  // TODO
		}
		else if (parts.at(0) == "time_left") {
			timeLeft = atof(parts.at(2).c_str());

			send(true, "=%s", id.c_str());  // TODO
		}
		else if (parts.at(0) == "list_commands") {
			send(true, "=%s name", id.c_str());
			send(true, "=%s version", id.c_str());
			send(true, "=%s boardsize", id.c_str());
			send(true, "=%s clear_board", id.c_str());
			send(true, "=%s play", id.c_str());
			send(true, "=%s genmove", id.c_str());
			send(true, "=%s komi", id.c_str());
			send(true, "=%s quit", id.c_str());
			send(true, "=%s loadsgf", id.c_str());
			send(true, "=%s final_score", id.c_str());
			send(true, "=%s time_settings", id.c_str());
			send(true, "=%s time_left", id.c_str());
		}
		else if (parts.at(0) == "final_score") {
			auto scores = score(*b);

			if (scores.first == scores.second)
				send(true, "=%s 0", id.c_str());
			else if (scores.first > scores.second)
				send(true, "=%s B+%d", id.c_str(), scores.first - scores.second);
			else
				send(true, "=%s W+%d", id.c_str(), scores.second - scores.first);
		}
		else if (parts.at(0) == "loadsgf") {
			delete b;
			b = new Board(loadSgf(parts.at(1)));

			send(true, "=%s", id.c_str());
		}
		else if (parts.at(0) == "autoplay") {
			player_t p = P_BLACK;

			uint64_t g_start_ts = get_ts_ms();
			int n_moves = 0;

			for(;;) {
				uint64_t start_ts = get_ts_ms();

				n_moves++;

				auto v = genMove(b, p, true, 1.0);
				if (v.has_value() == false)
					break;

				uint64_t end_ts = get_ts_ms();

				send(true, "# %s, took %.3fs/%d", v2t(v.value()).c_str(), (end_ts - start_ts) / 1000.0, n_moves);

				p = p == P_BLACK ? P_WHITE : P_BLACK;
			}

			uint64_t g_end_ts = get_ts_ms();

			send(true, "# finished %d moves in %.3fs", n_moves, (g_end_ts - g_start_ts) / 1000.0);
		}
		else if (parts.at(0) == "genmove" || parts.at(0) == "reg_genmove") {
			player_t player = (parts.at(1) == "b" || parts.at(1) == "black") ? P_BLACK : P_WHITE;

			if (timeLeft < 0)
				timeLeft = 5.0;

			uint64_t start_ts = get_ts_ms();
			auto v = genMove(b, player, parts.at(0) == "genmove", timeLeft);
			uint64_t end_ts = get_ts_ms();

			timeLeft = -1.0;

			if (v.has_value())
				send(true, "=%s %s", id.c_str(), v2t(v.value()).c_str());
			else
				send(true, "=%s pass", id.c_str());

			send(true, "# took %.3fs", (end_ts - start_ts) / 1000.0);
		}
		else if (parts.at(0) == "cputime") {
			struct rusage ru { 0 };

			if (getrusage(RUSAGE_SELF, &ru) == -1)
				send(true, "# getrusage failed");
			else {
				double usage = ru.ru_utime.tv_sec + ru.ru_utime.tv_usec / 1000000.0;

				send(true, "=%s %.4f", id.c_str(), usage);
			}
		}
		else {
			send(true, "?");
		}

		send(true, "");

//		dump(*b);

		fflush(nullptr);
	}

	delete b;
#endif
	fclose(fh);

	return 0;
}
