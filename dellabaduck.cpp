#include <algorithm>
#include <assert.h>
#include <atomic>
#include <climits>
#include <condition_variable>
#include <ctype.h>
#include <fstream>
#include <optional>
#include <queue>
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

//#define CALC_BCO

typedef enum { P_BLACK = 0, P_WHITE } player_t;
typedef enum { B_EMPTY, B_WHITE, B_BLACK, B_LAST } board_t;
const char *const board_t_names[] = { ".", "o", "x" };

FILE *fh = fopen("input.dat", "a+");

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

void dump(const std::set<Vertex, decltype(vertexCmp)> & set)
{
	send(true, "# Vertex set");

	std::string line = "# ";
	for(auto v : set)
		line += myformat("%s ", v2t(v).c_str());
	send(true, line.c_str());
}

void dump(const chain_t & chain)
{
	send(true, "# Chain for %s", board_t_names[chain.type]);

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

std::set<Vertex, decltype(vertexCmp)> stringToChain(const std::string & in, const int dim)
{
	auto parts = split(in, " ");

	std::set<Vertex, decltype(vertexCmp)> out;

	for(auto p : parts)
		out.insert(t2v(p, dim));

	return out;
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

		chain_t * getAt(const Vertex & v) const {
			return cm[v.getV()];
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

void findChainsScan(std::queue<std::pair<unsigned, unsigned> > *const work_queue, const Board & b, unsigned x, unsigned y, const int dx, const int dy, const board_t type, bool *const scanned)
{
	const unsigned dim = b.getDim();

	for(;;) {
		x += dx;
		y += dy;

		const unsigned v = y * dim + x;

		if (x >= dim || y >= dim || scanned[v])
			break;

		board_t type_at = b.getAt(v);

		if (type_at != type)
			break;

		work_queue->push({ x, y });
	}
}

void pickEmptyAround(const ChainMap & cm, const Vertex & v, std::set<Vertex, decltype(vertexCmp)> *const target)
{
	const int x = v.getX();
	const int y = v.getY();
	const int dim = cm.getDim();

	if (x > 0 && cm.getAt(x - 1, y) == nullptr)
		target->insert({ x - 1, y, dim });

	if (x < dim - 1 && cm.getAt(x + 1, y) == nullptr)
		target->insert({ x + 1, y, dim });

	if (y > 0 && cm.getAt(x, y - 1) == nullptr)
		target->insert({ x, y - 1, dim });

	if (y < dim - 1 && cm.getAt(x, y + 1) == nullptr)
		target->insert({ x, y + 1, dim });
}

void pickEmptyAround(const Board & b, const Vertex & v, std::set<Vertex, decltype(vertexCmp)> *const target)
{
	const int x = v.getX();
	const int y = v.getY();
	const int dim = b.getDim();

	if (x > 0 && b.getAt(x - 1, y) == B_EMPTY)
		target->insert({ x - 1, y, dim });

	if (x < dim - 1 && b.getAt(x + 1, y) == B_EMPTY)
		target->insert({ x + 1, y, dim });

	if (y > 0 && b.getAt(x, y - 1) == B_EMPTY)
		target->insert({ x, y - 1, dim });

	if (y < dim - 1 && b.getAt(x, y + 1) == B_EMPTY)
		target->insert({ x, y + 1, dim });
}

void findChains(const Board & b, std::vector<chain_t *> *const chainsWhite, std::vector<chain_t *> *const chainsBlack, ChainMap *const cm, const std::optional<board_t> ignore)
{
	const int dim = b.getDim();

	bool *scanned = new bool[dim * dim]();

	for(unsigned y=0; y<dim; y++) {
		for(unsigned x=0; x<dim; x++) {
			unsigned v = y * dim + x;

			if (scanned[v])
				continue;

			board_t bv = b.getAt(v);
			if (bv == B_EMPTY || (ignore.has_value() && ignore.value() == bv))
				continue;

			chain_t *curChain = new chain_t;
			curChain->type = bv;

			std::queue<std::pair<unsigned, unsigned> > work_queue;
			work_queue.push({ x, y });

			do {
				auto pair = work_queue.front();
				work_queue.pop();

				const unsigned x = pair.first;
				const unsigned y = pair.second;

				if (x >= dim || y >= dim)
					continue;

				const unsigned v = y * dim + x;

				board_t cur_bv = b.getAt(v);

				if (cur_bv == bv && scanned[v] == false) {
					scanned[v] = true;

					cm->setAt(x, y, curChain);

					curChain->chain.insert({ int(x), int(y), dim });

					findChainsScan(&work_queue, b, x, y, 0, -1, cur_bv, scanned);
					findChainsScan(&work_queue, b, x, y, 0, +1, cur_bv, scanned);
					findChainsScan(&work_queue, b, x, y, -1, 0, cur_bv, scanned);
					findChainsScan(&work_queue, b, x, y, +1, 0, cur_bv, scanned);
				}
			}
			while(work_queue.empty() == false);

			for(auto & stone : curChain->chain)
				pickEmptyAround(b, stone, &curChain->freedoms);

			if (curChain->type == B_WHITE)
				chainsWhite->push_back(curChain);
			else if (curChain->type == B_BLACK)
				chainsBlack->push_back(curChain);
		}
	}

	delete [] scanned;
}

void findLiberties(const ChainMap & cm, std::set<Vertex, decltype(vertexCmp)> *const empties, const board_t for_whom)
{
	const int dim = cm.getDim();

	for(int y=0; y<dim; y++) {
		for(int x=0; x<dim; x++) {
			if (cm.getAt(x, y) != nullptr)
				continue;

			bool ok = false;

			if (x > 0) {
				Vertex v(x - 1, y, dim);

				ok |= cm.getAt(v) == nullptr;
			}

			if (x < dim - 1) {
				Vertex v(x + 1, y, dim);

				ok |= cm.getAt(v) == nullptr;
			}

			if (y > 0) {
				Vertex v(x, y - 1, dim);

				ok |= cm.getAt(v) == nullptr;
			}

			if (y < dim - 1) {
				Vertex v(x, y + 1, dim);

				ok |= cm.getAt(v) == nullptr;
			}

			if (ok == false) {
				if (x > 0) {
					Vertex v(x - 1, y, dim);
					auto p = cm.getAt(v);

					ok |= p == nullptr || (p != nullptr && p->type == for_whom && p->freedoms.size() > 1);
				}

				if (x < dim - 1) {
					Vertex v(x + 1, y, dim);
					auto p = cm.getAt(v);

					ok |= p == nullptr || (p != nullptr && p->type == for_whom && p->freedoms.size() > 1);
				}

				if (y > 0) {
					Vertex v(x, y - 1, dim);
					auto p = cm.getAt(v);

					ok |= p == nullptr || (p != nullptr && p->type == for_whom && p->freedoms.size() > 1);
				}

				if (y < dim - 1) {
					Vertex v(x, y + 1, dim);
					auto p = cm.getAt(v);

					ok |= p == nullptr || (p != nullptr && p->type == for_whom && p->freedoms.size() > 1);
				}
			}

			if (ok)
				empties->insert({ x, y, dim });
		}
	}
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
			bool *scanned = new bool[dim * dim]();  // TODO: replace by memset

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

void connect(Board *const b, ChainMap *const cm, std::vector<chain_t *> *const chainsWhite, std::vector<chain_t *> *const chainsBlack, const board_t what, const int x, const int y)
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
		pickEmptyAround(*cm, v, &toMerge.at(0)->freedoms);
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
	board_t stone_type = playerToStone(p);

	b->setAt(v, stone_type);

	ChainMap cm(b->getDim());

	std::vector<chain_t *> chainsWhite, chainsBlack;

	// sanity check
#if !defined(NDEBUG)
#warning DEBUG enabled
	findChains(*b, &chainsWhite, &chainsBlack, &cm, { });

	std::vector<chain_t *> & scan_me = p == P_WHITE ? chainsWhite : chainsBlack;

	bool ok = true;
	for(auto chain : scan_me) {
		if (chain->freedoms.empty()) {
			dump(*b);
			dump(*chain);
			ok = false;
		}
	}

	assert(ok);
#else
	findChains(*b, &chainsWhite, &chainsBlack, &cm, p == P_BLACK ? B_WHITE : B_BLACK);
#endif

	std::vector<chain_t *> & scan = p == P_BLACK ? chainsWhite : chainsBlack;

	for(auto chain : scan) {
		if (chain->freedoms.empty()) {
			for(auto ve : chain->chain) {
				b->setAt(ve, B_EMPTY);
				send(false, "# purge %s", v2t(ve).c_str());
			}
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

inline bool isValidMove(const std::vector<chain_t *> & liberties, const Vertex & v)
{
	for(auto chain : liberties) {
		if (chain->freedoms.find(v) != chain->freedoms.end())
			return true;
	}

	return false;
}

// valid & not enclosed
bool isUsable(const ChainMap & cm, const std::vector<chain_t *> & liberties, const Vertex & v)
{
	return isValidMove(liberties, v) && cm.getEnclosed(v.getV()) == false;
}

void selectRandom(const Board & b, const ChainMap & cm, const std::vector<chain_t *> & chainsWhite, const std::vector<chain_t *> & chainsBlack, const player_t & p, std::vector<eval_t> *const evals)
{
	const std::vector<chain_t *> & myLiberties = p == P_BLACK ? chainsBlack : chainsWhite;

	size_t chainNr   = rand() % myLiberties.size();

	size_t chainSize = myLiberties.at(chainNr)->freedoms.size();

	int r = chainSize > 0 ? rand() % (chainSize - 1) : 0;

	auto it = myLiberties.at(chainNr)->freedoms.begin();
	for(int i=0; i<r; i++)
		it++;

	const int v = it->getV();

	evals->at(v).score++;
	evals->at(v).valid = true;
}

void selectExtendChains(const Board & b, const ChainMap & cm, const std::vector<chain_t *> & chainsWhite, const std::vector<chain_t *> & chainsBlack, const player_t & p, std::vector<eval_t> *const evals)
{
	const std::vector<chain_t *> & scan = p == P_BLACK ? chainsWhite : chainsBlack;

	const std::vector<chain_t *> & myLiberties = p == P_BLACK ? chainsBlack : chainsWhite;

	for(auto chain : scan) {
		for(auto chainStone : chain->chain) {
			std::set<Vertex, decltype(vertexCmp)> empties;
			pickEmptyAround(cm, chainStone, &empties);

			for(auto cross : empties) {
				if (isUsable(cm, myLiberties, cross)) {  // TODO: redundant check?
					int v = cross.getV();
					evals->at(v).score += 2;
					evals->at(v).valid = true;
				}
			}
		}
	}
}

void selectKillChains(const Board & b, const ChainMap & cm, const std::vector<chain_t *> & chainsWhite, const std::vector<chain_t *> & chainsBlack, const player_t & p, std::vector<eval_t> *const evals)
{
	const std::vector<chain_t *> & scan = p == P_BLACK ? chainsBlack : chainsWhite;
	const std::vector<chain_t *> & myLiberties = p == P_BLACK ? chainsBlack : chainsWhite;

	for(auto chain : scan) {
		const int add = chain->chain.size() - chain->freedoms.size();

		for(auto stone : chain->freedoms) {
			if (isUsable(cm, myLiberties, stone)) {
				int v = stone.getV();
				evals->at(v).score += add;
				evals->at(v).valid = true;
			}
		}
	}
}

void selectAtLeastOne(const Board & b, const ChainMap & cm, const std::vector<chain_t *> & chainsWhite, const std::vector<chain_t *> & chainsBlack, const player_t & p, std::vector<eval_t> *const evals)
{
	const std::vector<chain_t *> & myLiberties = p == P_BLACK ? chainsBlack : chainsWhite;

	if (myLiberties.empty())
		return;

	auto      it = myLiberties.at(0)->freedoms.begin();
	const int v = it->getV();

	evals->at(v).score++;
	evals->at(v).valid = true;
}

int calcN(const std::vector<chain_t *> & chains)
{
	int n = 0;

	for(auto chain : chains)
		n += chain->chain.size();

	return n;
}

int calcNLiberties(const std::vector<chain_t *> & chains)
{
	int n = 0;

	for(auto chain : chains)
		n += chain->freedoms.size();

	return n;
}

typedef struct
{
	std::atomic_bool        flag;
	std::condition_variable cv;
}
end_indicator_t;

#ifdef CALC_BCO
double bco_total = 0;
uint64_t bco_n = 0;
#endif

int search(const Board & b, const player_t & p, int alpha, const int beta, const int depth, const double komi, const uint64_t end_t, end_indicator_t *const ei, std::atomic_bool *const quick_stop)
{
	if (ei->flag || *quick_stop)
		return -32767;

	if (depth == 0) {
		auto s = score(b, komi);
		return p == P_BLACK ? s.first - s.second : s.second - s.first;
	}

	ChainMap cm(b.getDim());
	std::vector<chain_t *> chainsWhite, chainsBlack;
	findChains(b, &chainsWhite, &chainsBlack, &cm, { });
//	scanEnclosed(b, &cm, playerToStone(p));

	std::set<Vertex, decltype(vertexCmp)> liberties;
	findLiberties(cm, &liberties, playerToStone(p));

	// no valid freedoms? return score (eval)
	if (liberties.empty()) {
		purgeChains(&chainsBlack);
		purgeChains(&chainsWhite);

		auto s = score(b, komi);
		return p == P_BLACK ? s.first - s.second : s.second - s.first;
	}

	int bestScore = -32768;
	std::optional<Vertex> bestMove;

	player_t opponent = getOpponent(p);

#ifdef CALC_BCO
	int bco = 0;
#endif

	for(auto stone : liberties) {
		// TODO: check if in freedoms van de mogelijke crosses van p
#ifdef CALC_BCO
		bco++;
#endif

		Board work(b);

		play(&work, stone, p);

		int score = -search(work, opponent, -beta, -alpha, depth - 1, komi, end_t, ei, quick_stop);

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

finished:
#ifdef CALC_BCO
	bco_total += double(bco) / nLiberties;
	bco_n++;
#endif

	purgeChains(&chainsBlack);
	purgeChains(&chainsWhite);

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

struct CompareCrossesSortHelper {
	const Board & b;
	const int dim;
	const player_t p;

	CompareCrossesSortHelper(const Board & b, const player_t & p) : b(b), dim(b.getDim()), p(p) {
	}

	int getScore(const int move) {
		Board work(b);

		play(&work, { move, dim }, p);

		auto s = score(work, 0.);

		return p == P_BLACK ? s.first - s.second : s.second - s.first;
	}

	bool operator()(int i, int j) {
		return getScore(i) > getScore(j);
	}
};

void selectAlphaBeta(const Board & b, const ChainMap & cm, const std::vector<chain_t *> & chainsWhite, const std::vector<chain_t *> & chainsBlack, std::set<Vertex, decltype(vertexCmp)> & liberties, const player_t & p, std::vector<eval_t> *const evals, const double useTime, const double komi, const int nThreads)
{
	const int dim = b.getDim();

	bool *valid = new bool[dim * dim]();

	int n_work = 0;

	std::vector<int> places_for_sort;

	for(auto & v : liberties)
		places_for_sort.push_back(v.getV()), n_work++;

	std::sort(places_for_sort.begin(), places_for_sort.end(), CompareCrossesSortHelper(b, p));

	send(false, "# work: %d, time: %f", n_work, useTime);

	uint64_t start_t = get_ts_ms();  // TODO: start of genMove()
	uint64_t hend_t  = start_t + useTime * 1000 / 2;
	uint64_t end_t   = start_t + useTime * 1000;

	end_indicator_t ei { false };
	std::thread *to_timer = new std::thread(timer, end_t - start_t, &ei);

	int depth = 1;
	bool ok = false;

	size_t n_bytes  = sizeof(int) * dim * dim;

	std::optional<int> global_best;

	int alpha = -32767;
	int beta  =  32767;

	while(get_ts_ms() < hend_t && depth <= dim * dim) {
		send(false, "# a/b depth: %d", depth);

		fifo<int> places(dim * dim + 1);

		// queue "work" for threads
		for(auto & v: places_for_sort)
			places.put(v);

		std::atomic_bool quick_stop { false };

		ok = false;

		std::vector<std::thread *> threads;

		std::vector<std::optional<std::pair<int, int> > > best;
		best.resize(nThreads);

		for(int i=0; i<nThreads; i++) {
			threads.push_back(new std::thread([hend_t, end_t, &places, dim, b, p, depth, komi, &ei, alpha, beta, &best, &quick_stop, i, &best, &ok] {
						int local_alpha = alpha;
						int local_beta  = beta;

						for(;;) {
							int time_left = hend_t - get_ts_ms();
							if (time_left <= 0 || ei.flag)
								break;

							auto v = places.try_get();

							if (v.has_value() == false) {
								ok = true;
								break;
							}

							Board work(b);

							play(&work, { v.value(), dim }, p);

							int score = search(work, p == P_BLACK ? P_WHITE : P_BLACK, local_alpha, local_beta, depth, komi, end_t, &ei, &quick_stop);

							if (ei.flag == false && score > local_alpha) {
								local_alpha = score;

								best[i] = { v.value(), score };

								if (score >= beta) {
									send(false, "BCO: %d %d %d\n", local_alpha, score, local_beta);
									quick_stop = true;
									ok = true;
									break;
								}
							}
						}
					}));
		}

		send(false, "# %zu threads", threads.size());

		while(threads.empty() == false) {
			(*threads.begin())->join();

			//send(false, "# thread terminated, %zu left", threads.size());

			delete *threads.begin();

			threads.erase(threads.begin());
		}

		int                best_score = -32767;
		std::optional<int> best_move;

		for(size_t i=0; i<best.size(); i++) {
			if (best.at(i).has_value() && best.at(i).value().second > best_score) {
				best_move  = best.at(i).value().first;
				best_score = best.at(i).value().second;

				send(false, "# thread %zu chose %s with score %d", i, v2t(Vertex(best_move.value(), dim)).c_str(), best_score);
			}
		}

		if (best_score > alpha)
			alpha = best_score;

		if (ok && ei.flag == false && best_move.has_value()) {
			global_best = best_move;

			send(false, "# Move selected for this depth: %s (%d)", v2t(Vertex(global_best.value(), dim)).c_str(), global_best);
		}

		depth++;
	}

	if (to_timer) {
		ei.flag = true;
		ei.cv.notify_one();

		to_timer->join();

		delete to_timer;
	}

	if (global_best.has_value()) {
		send(false, "# Move selected for %c by A/B: %s (reached depth: %d, completed: %d)", p == P_BLACK ? 'B' : 'W', v2t(Vertex(global_best.value(), dim)).c_str(), depth, ok);

		evals->at(global_best.value()).score += 10;
		evals->at(global_best.value()).valid = true;
	}

#ifdef CALC_BCO
	double factor = bco_total / bco_n;
	send(false, "# BCO at %.3f%%; move %d, n: %lu", factor * 100, int(factor * dim * dim), bco_n);
#endif

	delete [] valid;
}

std::optional<Vertex> genMove(Board *const b, const player_t & p, const bool doPlay, const double useTime, const double komi)
{
	dump(*b);

	const int dim = b->getDim();
	const int p2dim = dim * dim;

	// find chains of stones
	ChainMap cm(dim);
	std::vector<chain_t *> chainsWhite, chainsBlack;
	findChains(*b, &chainsWhite, &chainsBlack, &cm, { });

	scanEnclosed(*b, &cm, playerToStone(p));

	std::set<Vertex, decltype(vertexCmp)> liberties;
	findLiberties(cm, &liberties, playerToStone(p));

	// no valid freedoms? return "pass".
	if (liberties.empty()) {
		purgeChains(&chainsBlack);
		purgeChains(&chainsWhite);

		return { };
	}

	dump(cm);

	dump(chainsBlack);
	dump(chainsWhite);

	send(false, "# useTime: %f", useTime);

	std::vector<eval_t> evals;
	evals.resize(p2dim);

	// algorithms
	// FIXME	selectRandom(*b, cm, chainsWhite, chainsBlack, p, &evals);

//	selectExtendChains(*b, cm, chainsWhite, chainsBlack, liberties, p, &evals);

//	selectKillChains(*b, cm, chainsWhite, chainsBlack, liberties, p, &evals);

//	selectAtLeastOne(*b, cm, chainsWhite, chainsBlack, liberties, p, &evals);

//	if (useTime > 0.1)
		selectAlphaBeta(*b, cm, chainsWhite, chainsBlack, liberties, p, &evals, useTime, komi, std::thread::hardware_concurrency());

	// find best
	std::optional<Vertex> v;

	int bestScore = -32767;
	for(int i=0; i<p2dim; i++) {
		if (evals.at(i).score > bestScore && evals.at(i).valid) {
			Vertex temp { i, dim };

			if (liberties.find(temp) != liberties.end()) {
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

	return v;
}

std::tuple<double, double, int> playout(const Board & in, const double komi, player_t p)
{
	Board b(in);

	// find chains of stones
	ChainMap cm(b.getDim());
	std::vector<chain_t *> chainsWhite, chainsBlack;
	findChains(b, &chainsWhite, &chainsBlack, &cm, { });

	int mc = 0;

	bool pass[2] { false };

	for(;;) {
		const std::vector<chain_t *> & myLiberties = p == P_BLACK ? chainsBlack : chainsWhite;

		// no valid freedoms? return "pass".
		if (calcNLiberties(myLiberties) == 0) {
			pass[p] = true;

			if (pass[0] && pass[1])
				break;

			p = getOpponent(p);

			continue;
		}

		pass[p] = false;

		size_t chainIdx  = rand() % myLiberties.size();
		size_t chainSize = myLiberties.at(chainIdx)->freedoms.size();
		int r = chainSize > 1 ? rand() % (chainSize - 1) : 0;

		auto it = myLiberties.at(chainIdx)->freedoms.begin();
		for(int i=0; i<r; i++)
			it++;

		const int x = it->getX();
		const int y = it->getY();

		connect(&b, &cm, &chainsWhite, &chainsBlack, playerToStone(p), x, y);

		p = getOpponent(p);

		if (++mc == 250)
			break;
	}

	purgeChains(&chainsBlack);
	purgeChains(&chainsWhite);

	auto s = score(b, komi);

	return std::tuple<double, double, int>(s.first, s.second, mc);
}

double benchmark_1(const Board & in, const int ms, const double komi)
{
	send(true, "# starting benchmark 1: duration: %.3fs, board dimensions: %d, komi: %g", ms / 1000.0, in.getDim(), komi);

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

double benchmark_2(const Board & in, const int ms)
{
	send(true, "# starting benchmark 2: duration: %.3fs, board dimensions: %d", ms / 1000.0, in.getDim());

	uint64_t start = get_ts_ms();
	uint64_t end = 0;
	uint64_t n = 0;

	ChainMap cm(in.getDim());

	do {
		std::vector<chain_t *> chainsWhite, chainsBlack;
		findChains(in, &chainsWhite, &chainsBlack, &cm, { });

		purgeChains(&chainsBlack);
		purgeChains(&chainsWhite);

		n++;

		end = get_ts_ms();
	}
	while(end - start < ms);

	double pops = n * 1000. / (end - start);
	send(true, "# playouts (%d) per second: %f", n, pops);

	return pops;
}

double benchmark_3(const Board & in, const int ms)
{
	send(true, "# starting benchmark 3: duration: %.3fs, board dimensions: %d", ms / 1000.0, in.getDim());

	int      dim    = in.getDim();
	int      dimsq  = dim * dim;

	uint64_t start  = get_ts_ms();
	uint64_t end    = 0;
	uint64_t n      = 0;

	uint64_t end_ts = start + ms;

	std::atomic_bool quick_stop { false };

	end_indicator_t  ei { false };

	srand(101);

	do {
		Board work(dim);

		int nstones = (rand() % dimsq) + 1;

		for(int i=0; i<nstones; i++)
			work.setAt(rand() % dimsq, rand() & 1 ? B_WHITE : B_BLACK);

		search(work, P_BLACK, -32767, 32767, 4, 1.5, end_ts, &ei, &quick_stop);

		n++;

		end = get_ts_ms();
	}
	while(end - start < ms);

	double pops = n * 1000. / (end - start);
	send(true, "# playouts (%d) per second: %f", n, pops);

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

Board loadSgfFile(const std::string & filename)
{
	FILE *sfh = fopen(filename.c_str(), "r");
	if (!sfh) {
		send(true, "Cannot open %s", filename.c_str());
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

Board loadSgf(const std::string & in)
{
	int dim = 9;

	const char *SZ = strstr(in.c_str(), "SZ[");
	if (SZ)
		dim = atoi(SZ + 3);

	Board b(dim);

	size_t o = 0;

	while(o < in.size()) {
		if (in[o] == ';' || in[o] == '(' || in[o] == ')') {
			o++;
			continue;
		}

		auto p = in.find('[', o);
		if (p == std::string::npos) {
			send(false, "# [ missing");
			break;
		}

		std::string name = in.substr(o, p - o);

		std::optional<board_t> player;

		if (name == "B")
			player = B_BLACK;
		else if (name == "W")
			player = B_WHITE;

		auto p_end = in.find(']', p);

		if (p_end == std::string::npos) {
			send(false, "# ] missing");
			break;
		}

		if (player.has_value()) {
			std::string pos = in.substr(p + 1, p_end - (p + 1));

			if (pos.size() != 2) {
				send(false, "# coordinate not 2 characters");
				break;
			}

			char x = pos.at(0) - 'a';
			char y = pos.at(1) - 'a';

			b.setAt(int(x), int(y), player.value());
		}

		o = p_end + 1;
	}

	dump(b);

	return b;
}

bool compareChain(const std::set<Vertex, decltype(vertexCmp)> & a, const std::set<Vertex, decltype(vertexCmp)> & b)
{
	for(auto v : a) {
		if (b.find(v) == b.end())
			return false;
	}

	return true;
}

typedef enum { fc_stones, fc_freedoms } fc_search_t;

bool findChain(const std::vector<chain_t *> & chains, const std::set<Vertex, decltype(vertexCmp)> & search_for, const fc_search_t & type)
{
	for(auto c : chains) {
		if (type == fc_stones) {
			if (compareChain(c->chain, search_for))
				return true;
		}
		else if (type == fc_freedoms) {
			if (compareChain(c->freedoms, search_for))
				return true;
		}
	}

	return false;
}

void test()
{
	struct test_data {
		std::string b;
		int         dim;
		double      score;
		std::vector<std::pair<std::string, std::string> > white_chains;
		std::vector<std::pair<std::string, std::string> > black_chains;
	};

	std::vector<test_data> boards;

	boards.push_back({
			".....\n"
			".....\n"
			".....\n"
			".....\n"
			".....\n"
			, 5, -1.5,
			{ },
			{ },
			});

	boards.push_back({
			"xxxxxxx\n"
			"x.....x\n"
			"x.o...x\n"
			"x...xxx\n"
			"xxxx...\n"
			".....xx\n"
			".....x.\n"
			, 7, 19.5,
			{ { "C5", "C6 D5 B5 C4" } },
			{ { "G2 F2 F1", "G3 F3 E2 G1 E1" }, { "G7 F7 E7 D7 C7 B7 A7 G6 A6 G5 A5 G4 F4 E4 A4 D3 C3 B3 A3", "F6 E6 D6 C6 B6 F5 E5 B5 D4 C4 B4 G3 F3 E3 D2 C2 B2 A2" } }
			});

	boards.push_back({
			"xxxxxxx\n"
			"x.....x\n"
			"x.....x\n"
			"x...xxx\n"
			"xxxx...\n"
			".....xx\n"
			".o...x.\n"
			, 7, 19.5,
			{ { "B1", "B2 C1 A1" } },
			{ { "G2 F2 F1", "G3 F3 E2 G1 E1" }, { "G7 F7 E7 D7 C7 B7 A7 G6 A6 G5 A5 G4 F4 E4 A4 D3 C3 B3 A3", "F6 E6 D6 C6 B6 F5 E5 B5 D4 C4 B4 G3 F3 E3 D2 C2 B2 A2" } }
		       	});

	boards.push_back({
			"xxxxxxx..\n"
			"x.....x.x\n"
			"x.....x.o\n"
			"x...xxx..\n"
			"xxxx.....\n"
			".....xx..\n"
			".o...x...\n"
			"......ooo\n"
			".....o...\n"
			, 9, 15.5,
			{ { "F1", "F2 G1 E1" }, { "J2 H2 G2", "J3 H3 G3 F2 J1 H1 G1" }, { "B3", "B4 C3 A3 B2" }, { "J7", "H7 J6" } },
			{ { "G4 F4 F3", "G5 F5 H4 E4 G3 E3 F2" }, { "G9 F9 E9 D9 C9 B9 A9 G8 A8 G7 A7 G6 F6 E6 A6 D5 C5 B5 A5", "H9 H8 F8 E8 D8 C8 B8 H7 F7 E7 B7 H6 D6 C6 B6 G5 F5 E5 D4 C4 B4 A4" }, { "J8", "J9 H8" } }
		       	});

	boards.push_back({
			".........\n"
			"...o.....\n"
			"xxox.x...\n"
			"..o...x.x\n"
			".........\n"
			"...o.o.o.\n"
			".........\n"
			".........\n"
			".........\n"
			, 9, -1.5,
			{ { "D4", "D5 E4 C4 D3" }, { "F4", "F5 G4 E4 F3" }, { "H4", "H5 J4 G4 H3" }, { "C7 C6", "C8 D6 B6 C5" }, { "D8", "D9 E8 C8" } },
			{ { "G6", "G7 H6 F6 G5" }, { "J6", "J7 H6 J5" }, { "B7 A7", "B8 A8 B6 A6" }, { "D7", "E7 D6" }, { "F7", "F8 G7 E7 F6" } }
		       	});

	boards.push_back({
			"xx.xxxxox\n"
			"xooooxoox\n"
			"xoxo.ooxx\n"
			"oxoxooox.\n"
			"xoxoxoxoo\n"
			"oxoxoxoox\n"
			"xooooooox\n"
			"oooxoxoxx\n"
			"xoxooxoox\n"
			, 9, -9.5,
			{ },
			{ }
			});

	for(auto b : boards) {
		bool   ok   = true;
		Board  brd  = stringToBoard(b.b);

		auto   temp_score = score(brd, 1.5);
		double test_score = temp_score.first - temp_score.second;

		if (test_score != b.score)
			printf("expected score: %f, current: %f\n", b.score, test_score), ok = false;

		ChainMap cm(brd.getDim());
		std::vector<chain_t *> chainsWhite, chainsBlack;
		findChains(brd, &chainsWhite, &chainsBlack, &cm, { });

		scanEnclosed(brd, &cm, playerToStone(P_WHITE));

		if (b.white_chains.size() != chainsWhite.size())
			printf("white: number of chains mismatch\n"), ok = false;

		if (b.black_chains.size() != chainsBlack.size())
			printf("black: number of chains mismatch\n"), ok = false;

		for(auto ch : b.white_chains) {
			auto white_stones = stringToChain(ch.first, brd.getDim());

			if (findChain(chainsWhite, white_stones, fc_stones) == false)
				printf("white stones mismatch\n"), ok = false;
		}

		for(auto ch : b.white_chains) {
			auto white_freedoms = stringToChain(ch.second, brd.getDim());

			if (findChain(chainsWhite, white_freedoms, fc_freedoms) == false)
				printf("white freedoms mismatch for %s\n", ch.second.c_str()), ok = false;
		}

		for(auto ch : b.black_chains) {
			auto black_stones = stringToChain(ch.first, brd.getDim());

			if (findChain(chainsBlack, black_stones, fc_stones) == false)
				printf("black stones mismatch\n"), ok = false;
		}

		for(auto ch : b.black_chains) {
			auto black_freedoms = stringToChain(ch.second, brd.getDim());

			if (findChain(chainsBlack, black_freedoms, fc_freedoms) == false)
				printf("black freedoms mismatch for %s\n", ch.second.c_str()), ok = false;
		}

		if (!ok) {
			dump(brd);

			dump(chainsBlack);

			dump(chainsWhite);

			printf("---\n");
		}

		purgeChains(&chainsBlack);
		purgeChains(&chainsWhite);
	}
}

std::string init_sgf(const int dim)
{
	return "(;AP[DellaBaduck]SZ[" + myformat("%d", dim) + "]";
}

int main(int argc, char *argv[])
{
	int c = -1;
	while((c = getopt(argc, argv, "c")) != -1) {
		if (c == 'c')  // console
			cgos = false;
	}

	setbuf(stdout, nullptr);
	setbuf(stderr, nullptr);

	srand(time(nullptr));

	Board *b = new Board(9);

	double timeLeft = -1;

	double komi = 0.;

	int moves_executed = 0;
	int moves_total    = 9 * 9;

	std::string sgf = init_sgf(b->getDim());

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

			moves_executed = 0;
			moves_total    = dim * dim / 2;

			sgf = init_sgf(dim);
		}
		else if (parts.at(0) == "play" && parts.size() == 3) {
			player_t p = (parts.at(1) == "b" || parts.at(1) == "black") ? P_BLACK : P_WHITE;

			send(true, "=%s", id.c_str());

			if (str_tolower(parts.at(2)) != "pass") {
				Vertex v = t2v(parts.at(2), b->getDim());

				play(b, v, p);
			}

			sgf += myformat(";%c[%s]", p == P_BLACK ? 'B' : 'W', parts.at(2).c_str());

			send(false, "# %s)", sgf.c_str());
		}
		else if (parts.at(0) == "debug" && parts.size() == 2) {
			ChainMap cm(b->getDim());
			std::vector<chain_t *> chainsWhite, chainsBlack;
			findChains(*b, &chainsWhite, &chainsBlack, &cm, { });

			scanEnclosed(*b, &cm, playerToStone((parts.at(1) == "b" || parts.at(1) == "black") ? P_BLACK : P_WHITE));

			dump(*b);
			dump(chainsBlack);
			dump(chainsWhite);
			dump(cm);

			purgeChains(&chainsBlack);
			purgeChains(&chainsWhite);
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
		else if (parts.at(0) == "benchmark" && parts.size() == 3) {
			double pops = -1.;

			// play outs per second
			if (parts.at(2) == "1")
				pops = benchmark_1(*b, atoi(parts.at(1).c_str()), komi);
			else if (parts.at(2) == "2")
				pops = benchmark_2(*b, atoi(parts.at(1).c_str()));
			else if (parts.at(2) == "3")
				pops = benchmark_3(*b, atoi(parts.at(1).c_str()));

			send(true, "=%s %f", id.c_str(), pops);
		}
		else if (parts.at(0) == "komi") {
			komi = atof(parts.at(1).c_str());

			send(true, "=%s", id.c_str());  // TODO
							//
			sgf += "KM[" + parts.at(1) + "]";
		}
		else if (parts.at(0) == "time_settings") {
			send(true, "=%s", id.c_str());  // TODO
		}
		else if (parts.at(0) == "time_left" && parts.size() >= 3) {
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
		else if (parts.at(0) == "unittest") {
			test();
		}
		else if (parts.at(0) == "loadsgf") {
			delete b;
			b = new Board(loadSgfFile(parts.at(1)));

			send(true, "=%s", id.c_str());
		}
		else if (parts.at(0) == "setsgf") {
			delete b;
			b = new Board(loadSgf(parts.at(1)));

			send(true, "=%s", id.c_str());
		}
		else if (parts.at(0) == "autoplay" && parts.size() == 2) {
			player_t p = P_BLACK;

			double think_time = atof(parts.at(1).c_str());

			double time_left[] = { think_time, think_time };

			uint64_t g_start_ts = get_ts_ms();
			int n_moves = 0;

			for(;;) {
				n_moves++;

				uint64_t start_ts = get_ts_ms();

				double time_use = time_left[p] / (moves_total - moves_executed);

				if (++moves_executed >= moves_total)
					moves_total = (moves_total * 4) / 3;

				auto v = genMove(b, p, true, time_use, komi);

				uint64_t end_ts = get_ts_ms();

				time_left[p] -= (end_ts - start_ts) / 1000.;

				const char *color = p == P_BLACK ? "black" : "white";

				if (time_left[p] < 0)
					send(true, "# %s is out of time (%f)", color, time_left[p]);

				if (v.has_value() == false)
					break;

				double took = (end_ts - start_ts) / 1000.;

				send(true, "# %s (%s), time allocated: %.3f, took %.3fs (%.2f%%), move-nr: %d, time left: %.3f", v2t(v.value()).c_str(), color, time_use, took, took * 100 / time_use, n_moves, time_left[p]);

				p = getOpponent(p);
			}

			uint64_t g_end_ts = get_ts_ms();

			send(true, "# finished %d moves in %.3fs", n_moves, (g_end_ts - g_start_ts) / 1000.0);
		}
		else if (parts.at(0) == "genmove" || parts.at(0) == "reg_genmove") {
			player_t player = (parts.at(1) == "b" || parts.at(1) == "black") ? P_BLACK : P_WHITE;

			if (timeLeft < 0)
				timeLeft = 5.0;

			double time_use = timeLeft / (moves_total - moves_executed);

			if (++moves_executed >= moves_total)
				moves_total = (moves_total * 4) / 3;

			uint64_t start_ts = get_ts_ms();
			auto v = genMove(b, player, parts.at(0) == "genmove", time_use, komi);
			uint64_t end_ts = get_ts_ms();

			timeLeft = -1.0;

			if (v.has_value()) {
				send(true, "=%s %s", id.c_str(), v2t(v.value()).c_str());

				sgf += myformat(";%c[%s]", player == P_BLACK ? 'B' : 'W', v2t(v.value()).c_str());
			}
			else {
				send(true, "=%s pass", id.c_str());
			}

			send(false, "# %s)", sgf.c_str());

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

	fclose(fh);

	return 0;
}
