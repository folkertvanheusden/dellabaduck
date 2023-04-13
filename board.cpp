#include <assert.h>
#include <string.h>

#include "board.h"
#include "helpers.h"
#include "io.h"


const char *board_t_name(const board_t v)
{
	static const char *const board_t_names[] = { ".", "o", "x" };

	return board_t_names[v];
}

uint64_t Board::getHashForField(const int v)
{
	board_t stone = b[v];

	if (stone == B_EMPTY)
		return 0;

	return z->get(v, stone == B_BLACK);
}

Board::Board(Zobrist *const z, const int dim) : z(z), dim(dim), b(new board_t[dim * dim]())
{
	assert(dim & 1);

	z->setDim(dim);
}

Board::Board(Zobrist *const z, const std::string & str) : z(z)
{
	auto slash = str.find('/');

	dim = slash;
	b = new board_t[dim * dim]();

	z->setDim(dim);

	int str_o = 0;

	for(int y=dim - 1; y >= 0; y--) {
		for(int x=0; x<dim; x++) {
			char c = str[str_o];

			int  o = y * dim + x;

			if (c == 'w' || c == 'W')
				setAt(x, y, B_WHITE), hash ^= z->get(o, false);
			else if (c == 'b' || c == 'B')
				setAt(x, y, B_BLACK), hash ^= z->get(o, true);
			else
				assert(c == '.');

			str_o++;
		}

		str_o++;  // skip slash
	}
}

Board::Board(const Board & bIn) : z(bIn.z), dim(bIn.getDim()), b(new board_t[dim * dim])
{
	assert(dim & 1);

	bIn.getTo(b);

	hash = bIn.hash;
}

Board::~Board()
{
	delete [] b;
}

int Board::getDim() const
{
	return dim;
}

void Board::getTo(board_t *const bto) const
{
	memcpy(bto, b, dim * dim * sizeof(*b));
}

board_t Board::getAt(const int v) const
{
	assert(v < dim * dim);
	assert(v >= 0);
	return b[v];
}

board_t Board::getAt(const int x, const int y) const
{
	assert(x < dim && x >= 0);
	assert(y < dim && y >= 0);
	int v = y * dim + x;
	return b[v];
}

uint64_t Board::getHash() const
{
	return hash;
}

void Board::setAt(const int v, const board_t bv)
{
	assert(v < dim * dim);
	assert(v >= 0);

	hash ^= getHashForField(v);

	b[v] = bv;

	hash ^= getHashForField(v);
}

void Board::setAt(const Vertex & v, const board_t bv)
{
	int vd = v.getV();

	hash ^= getHashForField(vd);

	b[vd] = bv;

	hash ^= getHashForField(vd);
}

void Board::setAt(const int x, const int y, const board_t bv)
{
	assert(x < dim && x >= 0);
	assert(y < dim && y >= 0);
	int v = y * dim + x;

	hash ^= getHashForField(v);
	b[v] = bv;
	hash ^= getHashForField(v);
}

ChainMap::ChainMap(const int dim) : dim(dim), cm(new chain_t *[dim * dim]()), enclosed(new bool[dim * dim]())
{
	assert(dim & 1);
}

ChainMap::~ChainMap()
{
	delete [] enclosed;
	delete [] cm;
}

bool ChainMap::getEnclosed(const int v) const
{
	return enclosed[v];
}

int ChainMap::getDim() const
{
	return dim;
}

chain_t * ChainMap::getAt(const int v) const
{
	return cm[v];
}

chain_t * ChainMap::getAt(const Vertex & v) const
{
	return cm[v.getV()];
}

chain_t * ChainMap::getAt(const int x, const int y) const
{
	assert(x < dim && x >= 0);
	assert(y < dim && y >= 0);
	int v = y * dim + x;
	return cm[v];
}

void ChainMap::setEnclosed(const int v)
{
	enclosed[v] = true;
}

void ChainMap::setAt(const Vertex & v, chain_t *const chain)
{
	setAt(v.getX(), v.getY(), chain);
}

void ChainMap::setAt(const int x, const int y, chain_t *const chain)
{
	assert(x < dim && x >= 0);
	assert(y < dim && y >= 0);
	int v = y * dim + x;
	cm[v] = chain;
}

void findChainsScan(std::queue<std::pair<unsigned, unsigned> > *const work_queue, const Board & b, unsigned x, unsigned y, const int dx, const int dy, const board_t type, bool *const scanned)
{
	const unsigned dim = b.getDim();

	for(;;) {
		x += dx;
		y += dy;

		if (x >= dim || y >= dim)
			break;

		const unsigned v = y * dim + x;

		if (scanned[v])
			break;

		board_t type_at = b.getAt(v);

		if (type_at != type)
			break;

		work_queue->push({ x, y });
	}
}

void pickEmptyAround(const ChainMap & cm, const Vertex & v, std::set<Vertex> *const target)
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

void pickEmptyAround(const Board & b, const Vertex & v, std::set<Vertex> *const target)
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

void findChains(const Board & b, std::vector<chain_t *> *const chainsWhite, std::vector<chain_t *> *const chainsBlack, ChainMap *const cm)
{
	const unsigned dim = b.getDim();

	bool *scanned = new bool[dim * dim]();

	for(unsigned y=0; y<dim; y++) {
		for(unsigned x=0; x<dim; x++) {
			unsigned v = y * dim + x;

			if (scanned[v])
				continue;

			board_t bv = b.getAt(v);
			if (bv == B_EMPTY)
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

					curChain->chain.insert({ int(x), int(y), int(dim) });

					findChainsScan(&work_queue, b, x, y, 0, -1, cur_bv, scanned);
					findChainsScan(&work_queue, b, x, y, 0, +1, cur_bv, scanned);
					findChainsScan(&work_queue, b, x, y, -1, 0, cur_bv, scanned);
					findChainsScan(&work_queue, b, x, y, +1, 0, cur_bv, scanned);
				}
			}
			while(work_queue.empty() == false);

			for(auto & stone : curChain->chain)
				pickEmptyAround(b, stone, &curChain->liberties);

			if (curChain->type == B_WHITE)
				chainsWhite->push_back(curChain);
			else if (curChain->type == B_BLACK)
				chainsBlack->push_back(curChain);
			else {
				send(false, "# INTERNAL ERROR: %d is not valid for a stone type", curChain->type);
				exit(1);
			}
		}
	}

	delete [] scanned;
}

void findLiberties(const ChainMap & cm, std::vector<Vertex> *const empties, const board_t for_whom)
{
	const int dim = cm.getDim();

	for(int y=0; y<dim; y++) {
		for(int x=0; x<dim; x++) {
			if (cm.getAt(x, y) != nullptr)
				continue;

			bool ok = false;

			if (x > 0)
				ok |= cm.getAt({ x - 1, y, dim }) == nullptr;

			if (x < dim - 1)
				ok |= cm.getAt({ x + 1, y, dim }) == nullptr;

			if (y > 0)
				ok |= cm.getAt({ x, y - 1, dim }) == nullptr;

			if (y < dim - 1)
				ok |= cm.getAt({ x, y + 1, dim }) == nullptr;

			if (ok == false) {
				if (x > 0) {
					auto p = cm.getAt({ x - 1, y, dim });

					ok |= p != nullptr && p->type == for_whom && p->liberties.size() > 1;
				}

				if (x < dim - 1) {
					auto p = cm.getAt({ x + 1, y, dim });

					ok |= p != nullptr && p->type == for_whom && p->liberties.size() > 1;
				}

				if (y > 0) {
					auto p = cm.getAt({ x, y - 1, dim });

					ok |= p != nullptr && p->type == for_whom && p->liberties.size() > 1;
				}

				if (y < dim - 1) {
					auto p = cm.getAt({ x, y + 1, dim });

					ok |= p != nullptr && p->type == for_whom && p->liberties.size() > 1;
				}
			}

			if (ok == false) {
				if (x > 0) {
					auto p = cm.getAt({ x - 1, y, dim });

					ok |= p != nullptr && p->type != for_whom && p->liberties.size() == 1;
				}

				if (x < dim - 1) {
					auto p = cm.getAt({ x + 1, y, dim });

					ok |= p != nullptr && p->type != for_whom && p->liberties.size() == 1;
				}

				if (y > 0) {
					auto p = cm.getAt({ x, y - 1, dim });

					ok |= p != nullptr && p->type != for_whom && p->liberties.size() == 1;
				}

				if (y < dim - 1) {
					auto p = cm.getAt({ x, y + 1, dim });

					ok |= p != nullptr && p->type != for_whom && p->liberties.size() == 1;
				}
			}

			if (ok)
				empties->push_back({ x, y, dim });
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

		// first remove this liberty
		for(auto & chain : toMerge)
			chain->liberties.erase(v);

		// merge
		auto cleanChainSet = what == B_WHITE ? chainsWhite : chainsBlack;

		for(size_t i=1; i<toMerge.size(); i++) {
			toMerge.at(0)->chain.merge(toMerge.at(i)->chain);  // add stones

			toMerge.at(0)->liberties.merge(toMerge.at(i)->liberties);  // add empty crosses surrounding the chain

			// remove chain from chainset
			auto it = std::find(cleanChainSet->begin(), cleanChainSet->end(), toMerge.at(i));
			delete *it;
			cleanChainSet->erase(it);
		}

		// update cm
		if (toMerge.size() > 1) {
			auto workOn = toMerge.at(0);  // at this point, entry 1... have been merged in 0

			for(auto & stone : workOn->chain)
				cm->setAt(stone, workOn);
		}

		// add any new liberties
		pickEmptyAround(*cm, v, &toMerge.at(0)->liberties);
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
		else {
			send(false, "# INTERNAL ERROR: %d is not valid for a stone type", what);
			exit(1);
		}

		cm->setAt(x, y, curChain);

		// find any liberties around it
		pickEmptyAround(*cm, Vertex(x, y, dim), &curChain->liberties);
	}

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
	auto purgeChainSet = what == B_WHITE ? chainsBlack : chainsWhite;

	for(auto chain=toClean.begin(); chain!=toClean.end();) {
		if ((*chain)->liberties.empty()) {
			for(auto ve : (*chain)->chain) {
				b->setAt(ve, B_EMPTY);  // remove part of the chain
				cm->setAt(ve, nullptr);
			}

			delete *chain;

			purgeChainSet->erase(std::find(purgeChainSet->begin(), purgeChainSet->end(), *chain));

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

	findChains(*b, &chainsWhite, &chainsBlack, &cm);

	std::vector<chain_t *> & scan = p == P_BLACK ? chainsWhite : chainsBlack;

	for(auto chain : scan) {
		if (chain->liberties.empty()) {
			for(auto ve : chain->chain)
				b->setAt(ve, B_EMPTY);
		}
	}

	purgeChains(&chainsBlack);
	purgeChains(&chainsWhite);
}
