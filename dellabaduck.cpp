#include <algorithm>
#include <assert.h>
#include <atomic>
#include <climits>
#include <condition_variable>
#include <ctype.h>
#include <fstream>
#include <optional>
#include <set>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <thread>
#include <time.h>
#include <tuple>
#include <unistd.h>
#include <vector>
#include <sys/resource.h>
#include <sys/time.h>

#include "fifo.h"
#include "str.h"
#include "time.h"

typedef enum { P_BLACK = 0, P_WHITE } player_t;
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
	asprintf(&ts_str, "%04d-%02d-%02d %02d:%02d:%02d.%03d [%d] ",
			tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec, int(now % 1000), getpid());

	char *str = nullptr;

	va_list ap;
	va_start(ap, fmt);
	(void)vasprintf(&str, fmt, ap);
	va_end(ap);
	fflush(fh);

	if (tx) {
		if (cgos && fmt[0] == '#') {
			fprintf(fh, "%s%s\n", ts_str, str);
			return;
		}

		fprintf(fh, "%s|%s|\n", ts_str, str);

		printf("%s\n", str);
	}
	else {
		fprintf(fh, "%s%s\n", ts_str, str);
	}

	fflush(fh);

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
		assert(dim & 1);
	}

	Vertex(const int x, const int y, const int dim) : v(y * dim + x), dim(dim) {
		assert(dim & 1);
	}

	Vertex(const Vertex & v) : v(v.getV()), dim(v.getDim()) {
		assert(dim & 1);
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
	send(false, "# Chain for %s", board_t_names[chain.type]);

	std::string line = "# ";
	for(auto v : chain.chain) 
		line += myformat("%s ", v2t(v).c_str());
	send(false, line.c_str());

	if (chain.freedoms.empty() == false) {
		send(false, "# Freedoms of that chain:");

		line = "# ";
		for(auto v : chain.freedoms) 
			line += myformat("%s ", v2t(v).c_str());
		send(false, line.c_str());
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

		send(false, line.c_str());
	}

	line = "#      ";

	for(int x=0; x<dim; x++) {
		int xc = 'A' + x;

		if (xc >= 'I')
			xc++;

		line += myformat("%c", xc);
	}

	send(false, line.c_str());
}

class ChainMap {
private:
	const int dim;
	chain_t **const cm;
	bool *const enclosed;

public:
	ChainMap(const int dim) : dim(dim), cm(new chain_t *[dim * dim]()), enclosed(new bool[dim * dim]()) {
		assert(dim & 1);
	}

	~ChainMap() {
		delete [] enclosed;
		delete [] cm;
	}

	bool getEnclosed(const int v) const {
		return enclosed[v];
	}

	int getDim() const {
		return dim;
	}

	chain_t * getAt(const int v) const {
		return cm[v];
	}

	chain_t * getAt(const int x, const int y) const {
		assert(x < dim && x >= 0);
		assert(y < dim && y >= 0);
		int v = y * dim + x;
		return cm[v];
	}

	void setEnclosed(const int v) {
		enclosed[v] = true;
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

void dump(const ChainMap & cm)
{
	const int dim = cm.getDim();

	std::string line;

	for(int y=dim - 1; y>=0; y--) {
		line = myformat("# %2d | ", y + 1);

		for(int x=0; x<dim; x++)
			line += cm.getEnclosed(y * dim + x) ? '1' : '0';

		send(false, line.c_str());
	}

	line = "#      ";

	for(int x=0; x<dim; x++) {
		int xc = 'A' + x;

		if (xc >= 'I')
			xc++;

		line += myformat("%c", xc);
	}

	send(false, line.c_str());
}

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

void scanBoundaries(const Board & b, const ChainMap & cm, bool *const scanned, const board_t myStone, const int x, const int y, std::set<chain_t *> *const enclosedBy, bool *const undecided)
{
	const int dim = b.getDim();

	const int v = y * dim + x;

	board_t stone = b.getAt(x, y);

	if (scanned[v] == false) {
		scanned[v] = true;

		if (stone == B_EMPTY || stone == myStone) {
			if (y > 0 && *undecided == false)
				scanBoundaries(b, cm, scanned, myStone, x, y - 1, enclosedBy, undecided);
			else
				*undecided = true;
			if (y < dim - 1 && *undecided == false)
				scanBoundaries(b, cm, scanned, myStone, x, y + 1, enclosedBy, undecided);
			else
				*undecided = true;
			if (x > 0 && *undecided == false)
				scanBoundaries(b, cm, scanned, myStone, x - 1, y, enclosedBy, undecided);
			else
				*undecided = true;
			if (x < dim - 1 && *undecided == false)
				scanBoundaries(b, cm, scanned, myStone, x + 1, y, enclosedBy, undecided);
			else
				*undecided = true;
		}
		else {
			auto chain = cm.getAt(v);

			if (chain->type != myStone)
				enclosedBy->insert(chain);
		}
	}
}

void scanEnclosed(const Board & b, ChainMap *const cm, const board_t myType)
{
	const int dim = cm->getDim();

	for(int y=0; y<dim; y++) {
		for(int x=0; x<dim; x++) {
			bool *scanned = new bool[dim * dim]();

			std::set<chain_t *> enclosedBy;

			bool undecided = false;

			scanBoundaries(b, *cm, scanned, myType, x, y, &enclosedBy, &undecided);

			if (!undecided && enclosedBy.size() == 1)
				cm->setEnclosed(y * dim + x);

			delete [] scanned;
		}
	}
}

void purgeChains(std::vector<chain_t *> *const chains)
{
	for(auto chain : *chains)
		delete chain;

	chains->clear();
}

void purgeFreedoms(std::vector<chain_t *> *const chainsPurge, const ChainMap & cm, const board_t me)
{
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

std::vector<Vertex> pickEmptyAround(const ChainMap & cm, const Vertex & v)
{
	const int x = v.getX();
	const int y = v.getY();
	const int dim = cm.getDim();

	std::vector<Vertex> out;

	if (x > 0 && cm.getAt(x - 1, y) == nullptr)
		out.push_back({ x - 1, y, dim });

	if (x < dim - 1 && cm.getAt(x + 1, y) == nullptr)
		out.push_back({ x + 1, y, dim });

	if (y > 0 && cm.getAt(x, y - 1) == nullptr)
		out.push_back({ x, y - 1, dim });

	if (y < dim - 1 && cm.getAt(x, y + 1) == nullptr)
		out.push_back({ x, y + 1, dim });

	return out;
}

int countLiberties(const Board & b, const int x, const int y)
{
	const int dim = b.getDim();

	int n = 0;

	n += x >= 0      && b.getAt(x - 1, y) == B_EMPTY;
	n += x < dim - 1 && b.getAt(x + 1, y) == B_EMPTY;
	n += y >= 0      && b.getAt(x, y - 1) == B_EMPTY;
	n += y < dim - 1 && b.getAt(x, y + 1) == B_EMPTY;

	return n;
}

void connect(Board *const b, ChainMap *const cm, std::vector<chain_t *> *const chainsWhite, std::vector<chain_t *> *const chainsBlack, std::vector<chain_t *> *const chainsEmpty, const board_t what, const int x, const int y)
{
	const int dim = b->getDim();

	assert(x >= 0 && x < dim);
	assert(y >= 0 && y < dim);

	// update board
	assert(b->getAt(x, y) == B_EMPTY);
	b->setAt(x, y, what);

	// find chains to merge
	std::set<chain_t *> toMergeTemp;

	if (y > 0 && cm->getAt(x, y - 1) && cm->getAt(x, y - 1)->type == what)
		toMergeTemp.insert(cm->getAt(x, y - 1));

	if (y < dim - 1 && cm->getAt(x, y + 1) && cm->getAt(x, y + 1)->type == what)
		toMergeTemp.insert(cm->getAt(x, y + 1));

	if (x > 0 && cm->getAt(x - 1, y) && cm->getAt(x - 1, y)->type == what)
		toMergeTemp.insert(cm->getAt(x - 1, y));

	if (x < dim - 1 && cm->getAt(x + 1, y) && cm->getAt(x + 1, y)->type == what)
		toMergeTemp.insert(cm->getAt(x + 1, y));

	std::vector<chain_t *> toMerge;
	for(auto & chain : toMergeTemp)
		toMerge.push_back(chain);

	// add new piece to (existing) first chain (of the set of chains found to be merged)
	if (toMerge.empty() == false) {
		Vertex v(x, y, dim);

		// add to chain
		toMerge.at(0)->chain.insert(v);
		// update board->chain map
		cm->setAt(x, y, toMerge.at(0));

		// first remove this freedom
		for(auto & chain : toMerge)
			chain->freedoms.erase(v);

		// merge
		auto cleanChainSet = what == B_WHITE ? chainsWhite : chainsBlack;

		for(size_t i=1; i<toMerge.size(); i++) {
			toMerge.at(0)->chain.merge(toMerge.at(i)->chain);  // add stones

			toMerge.at(0)->freedoms.merge(toMerge.at(i)->freedoms);  // add empty crosses surrounding the chain

			// remove chain from chainset
			auto it = std::find(cleanChainSet->begin(), cleanChainSet->end(), toMerge.at(i));
			cleanChainSet->erase(it);
		}

		// update cm
		if (toMerge.size() > 1) {
			auto workOn = toMerge.at(0);  // at this point, entry 1... have been merged in 0

			for(auto & stone : workOn->chain)
				cm->setAt(stone, workOn);
		}

		// add any new liberties
		std::vector<Vertex> newFreedoms = pickEmptyAround(*cm, v);

		for(auto & cross : newFreedoms)
			toMerge.at(0)->freedoms.insert(cross);
	}
	else {
		// this is a new chain
		chain_t *curChain = new chain_t;
		curChain->type = what;
		curChain->chain.insert({ x, y, dim });

		if (what == B_WHITE)
			chainsWhite->push_back(curChain);
		else if (what == B_BLACK)
			chainsBlack->push_back(curChain);
		else
			assert(0);

		cm->setAt(x, y, curChain);
	}

	// erase liberties from opponent chains
	std::vector<chain_t *> *const scan = what == B_BLACK ? chainsWhite : chainsBlack;

	// find surrounding opponent chains
	std::set<chain_t *> toClean;

	if (y > 0 && cm->getAt(x, y - 1) && cm->getAt(x, y - 1)->type != what)
		toClean.insert(cm->getAt(x, y - 1));

	if (y < dim - 1 && cm->getAt(x, y + 1) && cm->getAt(x, y + 1)->type != what)
		toClean.insert(cm->getAt(x, y + 1));

	if (x > 0 && cm->getAt(x - 1, y) && cm->getAt(x - 1, y)->type != what)
		toClean.insert(cm->getAt(x - 1, y));

	if (x < dim - 1 && cm->getAt(x + 1, y) && cm->getAt(x + 1, y)->type != what)
		toClean.insert(cm->getAt(x + 1, y));

	// remove chains without liberties
	for(auto chain=toClean.begin(); chain!=toClean.end();) {
		if ((*chain)->freedoms.empty()) {
			for(auto ve : (*chain)->chain)
				b->setAt(ve, B_EMPTY);

			chain = toClean.erase(chain);
		}
		else {
			chain++;
		}
	}
}

void play(Board *const b, const Vertex & v, const player_t & p)
{
	b->setAt(v, playerToStone(p));

	ChainMap cm(b->getDim());
	std::vector<chain_t *> chainsWhite, chainsBlack;
	findChains(*b, &chainsWhite, &chainsBlack, &cm);

	std::vector<chain_t *> & scan = p == P_BLACK ? chainsWhite : chainsBlack;

	for(auto chain : scan) {
		if (chain->freedoms.empty()) {
			for(auto ve : chain->chain)
				b->setAt(ve, B_EMPTY);
		}
	}

	purgeChains(&chainsBlack);
	purgeChains(&chainsWhite);
}

void scoreFloodFill(const Board & b, const int dim, bool *const reachable, const int x, const int y, const board_t lookFor)
{
	auto piece = b.getAt(x, y);

	int v = y * dim + x;

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
	const int dim = b.getDim();
	const int dimsq = dim * dim;

	int blackStones = 0;
	int whiteStones = 0;
	bool *reachableBlack = new bool[dimsq]();
	bool *reachableWhite = new bool[dimsq]();

	for(int y=0; y<dim; y++) {
		for(int x=0; x<dim; x++) {
			auto piece = b.getAt(x, y);

			if (piece == B_BLACK)
				scoreFloodFill(b, dim, reachableBlack, x, y, B_BLACK);
			else if (piece == B_WHITE)
				scoreFloodFill(b, dim, reachableWhite, x, y, B_WHITE);
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

std::string scoreStr(auto scores)
{
	if (scores.first > scores.second)
		return myformat("B+%g", scores.first - scores.second);

	if (scores.first < scores.second)
		return myformat("W+%g", scores.second - scores.first);

	return "0";
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

// valid & not enclosed
bool isUsable(const ChainMap & cm, const std::vector<chain_t *> & chainsEmpty, const Vertex & v)
{
	return isValidMove(chainsEmpty, v) && cm.getEnclosed(v.getV()) == false;
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

void selectExtendChains(const Board & b, const ChainMap & cm, const std::vector<chain_t *> & chainsWhite, const std::vector<chain_t *> & chainsBlack, const std::vector<chain_t *> & chainsEmpty, const player_t & p, std::vector<eval_t> *const evals)
{
	const std::vector<chain_t *> & scan = p == P_BLACK ? chainsWhite : chainsBlack;

	for(auto chain : scan) {
		for(auto chainStone : chain->chain) {
			auto empties = pickEmptyAround(cm, chainStone);

			for(auto cross : empties) {
				if (isUsable(cm, chainsEmpty, cross)) {
					int v = cross.getV();
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
			if (isUsable(cm, chainsEmpty, stone)) {
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

typedef struct
{
        std::atomic_bool        flag;
        std::condition_variable cv;
}
end_indicator_t;

int search(const Board & b, const player_t & p, int alpha, const int beta, const int depth, const int komi, const uint64_t end_t, end_indicator_t *const ei)
{
	if (ei->flag)
		return -32767;

	const int dim = b.getDim();

	std::vector<chain_t *> chainsEmpty;

        std::vector<eval_t> evals;

	// do not calculate chains when at depth 0 as they're not used then
	if (depth > 0) {
		ChainMap cm(dim);
		std::vector<chain_t *> chainsWhite, chainsBlack;
		findChains(b, &chainsWhite, &chainsBlack, &cm);

		// find chains of freedoms
		findChainsOfFreedoms(b, &chainsEmpty);
		purgeFreedoms(&chainsEmpty, cm, playerToStone(p));

		if (depth == 1) {
			evals.resize(dim * dim);

			selectExtendChains(b, cm, chainsWhite, chainsBlack, chainsEmpty, p, &evals);
			selectKillChains(b, cm, chainsWhite, chainsBlack, chainsEmpty, p, &evals);
		}

		purgeChains(&chainsBlack);
		purgeChains(&chainsWhite);
	}

	// no valid freedoms? return score (eval)
	if (chainsEmpty.empty()) {
		auto s = score(b, komi);
		return p == P_BLACK ? s.first - s.second : s.second - s.first;
	}

	if (get_ts_ms() >= end_t)
		return -INT_MAX;

	int bestScore = -32768;
	std::optional<Vertex> bestMove;

	player_t opponent = getOpponent(p);

	for(auto chain : chainsEmpty) {
		for(auto stone : chain->chain) {
			// TODO: isUsable()

			Board work(b);

			play(&work, stone, p);

			int score = -search(work, opponent, -beta, -alpha, depth - 1, komi, end_t, ei);

			if (depth == 1)
				score += evals.at(stone.getV()).score;

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

void timer(int think_time, end_indicator_t *const ei)
{
	if (think_time > 0) {
		std::mutex m;
		std::unique_lock<std::mutex> lk(m);

		for(;;) {
			if (ei->cv.wait_for(lk, std::chrono::milliseconds(think_time)) == std::cv_status::timeout) {
				ei->flag = true;
				break;
			}

			if (ei->flag)
				break;
		}
	}
	else {
		ei->flag = true;
	}
}

void selectAlphaBeta(const Board & b, const ChainMap & cm, const std::vector<chain_t *> & chainsWhite, const std::vector<chain_t *> & chainsBlack, const std::vector<chain_t *> & chainsEmpty, const player_t & p, std::vector<eval_t> *const evals, const double useTime, const double komi, const int nThreads)
{
	const int dim = b.getDim();

	bool *valid = new bool[dim * dim]();

	for(int i=0; i<dim * dim; i++)
		valid[i] = isUsable(cm, chainsEmpty, { i, dim });

	uint64_t start_t = get_ts_ms();  // TODO: start of genMove()
	uint64_t hend_t  = start_t + useTime * 1000 / 2;
	uint64_t end_t   = start_t + useTime * 1000;

	end_indicator_t ei { false };
	std::thread *to_timer = new std::thread(timer, end_t, &ei);

	int depth = 1;

	size_t n_bytes  = sizeof(int) * dim * dim;

	int *selected_scores = reinterpret_cast<int *>(calloc(1, n_bytes));

	while(get_ts_ms() < hend_t) {
		send(false, "# a/b depth: %d", depth);

		fifo<int> places(dim * dim + 1);

		// queue "work" for threads
		for(int i=0; i<dim * dim; i++) {
			if (valid[i])
				places.put(i);
		}

		std::vector<std::thread *> threads;

		int *scores = reinterpret_cast<int *>(calloc(1, n_bytes));

		bool to = false;

		for(int i=0; i<nThreads; i++) {
			threads.push_back(new std::thread([hend_t, end_t, &places, dim, scores, b, p, depth, komi, &to, &ei] {
				for(;;) {
					int time_left = hend_t - get_ts_ms();
					if (time_left <= 0)
						break;

					auto v = places.try_get();

					if (v.has_value() == false || ei.flag)
						break;

					Board work(b);

					play(&work, { v.value(), dim }, p);

					int score = search(work, p == P_BLACK ? P_WHITE : P_BLACK, -32768, 32768, depth, komi, end_t, &ei);

					if (ei.flag == false)
						scores[v.value()] = score;
				}
			}));
		}

		send(false, "# %zu threads\n", threads.size());

		while(threads.empty() == false) {
			(*threads.begin())->join();

			send(false, "# thread terminated, %zu left\n", threads.size());

			delete *threads.begin();

			threads.erase(threads.begin());
		}

		if (places.is_empty() == true && to == false)
			memcpy(selected_scores, scores, n_bytes);
		else
			send(false, "# not enough time\n");

		free(scores);

		depth++;
	}

	int best  = -INT_MAX;
	int besti = -1;

	for(int i=0; i<dim * dim; i++) {
		evals->at(i).score += selected_scores[i];
		evals->at(i).valid = true;

		if (selected_scores[i] > best) {
			best  = selected_scores[i];
			besti = i;
		}
	}

	free(selected_scores);
	
	if (to_timer) {
		ei.flag = true;
		ei.cv.notify_one();

		to_timer->join();

		delete to_timer;
	}

	if (besti != -1) {
		Vertex v(besti, dim);

		send(false, "# Move selected by A/B: %s (%d)", v2t(v).c_str(), best);

		evals->at(besti).score += 5;
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

std::optional<Vertex> genMove(Board *const b, const player_t & p, const bool doPlay, const double timeLeft, const double komi)
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

	scanEnclosed(*b, &cm, playerToStone(p));
	dump(cm);

	size_t totalNChains = chainsWhite.size() + chainsBlack.size();

	double useTime = (timeLeft / 2.) * totalNChains / p2dim * 0.95;

	send(false, "# timeLeft: %f, useTime: %f, total chain-count: %zu, board dimension: %d", timeLeft, useTime, totalNChains, dim);

        std::vector<eval_t> evals;
	evals.resize(p2dim);

	// algorithms
// FIXME	selectRandom(*b, cm, chainsWhite, chainsBlack, chainsEmpty, p, &evals);

	selectExtendChains(*b, cm, chainsWhite, chainsBlack, chainsEmpty, p, &evals);

	selectKillChains(*b, cm, chainsWhite, chainsBlack, chainsEmpty, p, &evals);

	selectAtLeastOne(*b, cm, chainsWhite, chainsBlack, chainsEmpty, p, &evals);

	selectAlphaBeta(*b, cm, chainsWhite, chainsBlack, chainsEmpty, p, &evals, useTime, komi, std::thread::hardware_concurrency());

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

		send(false, line.c_str());
	}

	std::string line = "#      ";
	for(int x=0; x<dim; x++) {
		char c = 'A' + x;
		if (c >= 'I')
			c++;

		line += myformat(" %c  ", c);
	}

	send(false, line.c_str());

	// remove any chains that no longer have freedoms after this move
	// also play the move
	if (doPlay && v.has_value())
		play(b, v.value(), p);

	purgeChains(&chainsBlack);
	purgeChains(&chainsWhite);
	purgeChains(&chainsEmpty);

	return v;
}

std::tuple<double, double, int> playout(const Board & in, const double komi, player_t p)
{
	Board b(in);

	// find chains of stones
	ChainMap cm(b.getDim());
	std::vector<chain_t *> chainsWhite, chainsBlack;
	findChains(b, &chainsWhite, &chainsBlack, &cm);

	// find chains of freedoms
	std::vector<chain_t *> chainsEmpty;
	findChainsOfFreedoms(b, &chainsEmpty);
	purgeFreedoms(&chainsEmpty, cm, playerToStone(p));

	int mc = 0;

	bool pass[2] { false };

	for(;;) {
		// no valid freedoms? return "pass".
		if (chainsEmpty.empty()) {
			purgeChains(&chainsBlack);
			purgeChains(&chainsWhite);

			pass[p] = true;

			if (pass[0] && pass[1])
				break;

			p = getOpponent(p);

			continue;
		}

		pass[p] = false;

		auto chain = chainsEmpty.at(rand() % chainsEmpty.size());
		size_t chainSize = chain->chain.size();
		int r = rand() % (chainSize - 1);

		auto it = chain->chain.begin();
		for(int i=0; i<r; i++)
			it++;

		const int x = it->getX();
		const int y = it->getY();

		connect(&b, &cm, &chainsWhite, &chainsBlack, &chainsEmpty, playerToStone(p), x, y);

		p = getOpponent(p);

		if (++mc == 250)
			break;

		// TODO: move this to connect() as well
		purgeChains(&chainsEmpty);

		findChainsOfFreedoms(b, &chainsEmpty);
		purgeFreedoms(&chainsEmpty, cm, playerToStone(p));
	}

	purgeChains(&chainsEmpty);
	purgeChains(&chainsBlack);
	purgeChains(&chainsWhite);

	auto s = score(b, komi);

	return std::tuple<double, double, int>(s.first, s.second, mc);
}

double benchmark(const Board & in, const int ms, const double komi)
{
	send(true, "# starting benchmark: duration: %.3fs, board dimensions: %d, komi: %g", ms / 1000.0, in.getDim(), komi);

	uint64_t start = get_ts_ms();
	uint64_t end = 0;
	uint64_t n = 0;
	uint64_t total_puts = 0;

	do {
		auto result = playout(in, komi, P_BLACK);

		total_puts += std::get<2>(result);

		n++;

		end = get_ts_ms();
	}
	while(end - start < ms);

	double pops = n * 1000. / (end - start);
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
        Board b5 = stringToBoard(
                        ".....\n"
                        ".....\n"
                        ".....\n"
                        ".....\n"
                        ".....\n"
                        );

        Board b7_1 = stringToBoard(
                        "xxxxxxx\n"
                        "x.....x\n"
                        "x.o...x\n"
                        "x...xxx\n"
                        "xxxx...\n"
                        ".....xx\n"
                        ".....x.\n"
                        );

        Board b7_2 = stringToBoard(
                        "xxxxxxx\n"
                        "x.....x\n"
                        "x.....x\n"
                        "x...xxx\n"
                        "xxxx...\n"
                        ".....xx\n"
                        ".o...x.\n"
                        );

	Board & b = b7_2;

#if 0
	auto scores = score(b);

	std::string id;
	if (scores.first == scores.second)
		printf("=%s 0\n\n", id.c_str());
	else if (scores.first > scores.second)
		printf("=%s B+%d\n\n", id.c_str(), scores.first - scores.second);
	else
		printf("=%s W+%d\n\n", id.c_str(), scores.second - scores.first);

	return 0;

#endif
        dump(b);

	ChainMap cm(b.getDim());
        std::vector<chain_t *> chainsWhite, chainsBlack;
        findChains(b, &chainsWhite, &chainsBlack, &cm);

	scanEnclosed(b, &cm, playerToStone(P_WHITE));
	dump(cm);

	dump(chainsWhite);

	return 0;

	send(false, "#");
	send(false, "# white:");
        dump(chainsWhite);

	send(false, "#");
	send(false, "# black:");
        dump(chainsBlack);

	std::vector<chain_t *> chainsEmpty;
	findChainsOfFreedoms(b, &chainsEmpty);
	send(false, "#");
	send(false, "# empty:");
	dump(chainsEmpty);

	purgeFreedoms(&chainsEmpty, cm, B_BLACK);
	send(false, "#");
	send(false, "# empty after purge:");
	dump(chainsEmpty);

	purgeChains(&chainsBlack);
	purgeChains(&chainsWhite);
	purgeChains(&chainsEmpty);
#endif
#if 0
	dump(b);
	auto v = genMove(b, P_BLACK, true, 0);
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

	double komi = 0.;

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
				play(b, v, (parts.at(1) == "b" || parts.at(1) == "black") ? P_BLACK : P_WHITE);
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
			double pops = benchmark(*b, parts.size() == 2 ? atoi(parts.at(1).c_str()) : 1000, komi);

			send(true, "=%s %f", id.c_str(), pops);
		}
		else if (parts.at(0) == "komi") {
			komi = atof(parts.at(1).c_str());

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
			send(true, "=%s %s", id.c_str(), scoreStr(score(*b, komi)).c_str());
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

				auto v = genMove(b, p, true, timeLeft, komi);
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
			auto v = genMove(b, player, parts.at(0) == "genmove", timeLeft, komi);
			uint64_t end_ts = get_ts_ms();

			timeLeft = -1.0;

			if (v.has_value())
				send(true, "=%s %s", id.c_str(), v2t(v.value()).c_str());
			else
				send(true, "=%s pass", id.c_str());

			send(false, "# took %.3fs", (end_ts - start_ts) / 1000.0);
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
