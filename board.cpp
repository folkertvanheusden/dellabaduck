#include <assert.h>
#include <string.h>

#include "board.h"
#include "dump.h"
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

void pickEmptyAround(const ChainMap & cm, const Vertex & v, std::unordered_set<Vertex, Vertex::HashFunction> *const target)
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

void pickEmptyAround(const Board & b, const Vertex & v, std::unordered_set<Vertex, Vertex::HashFunction> *const target)
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

	assert(chainsWhite->empty());
	assert(chainsBlack->empty());

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

					curChain->chain.push_back({ int(x), int(y), int(dim) });

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

void checkLiberty(const ChainMap & cm, const int x, const int y, const board_t for_whom, std::unordered_set<Vertex, Vertex::HashFunction> *const target)
{
	bool      ok    = false;
	const int dim   = cm.getDim();
	const int dimm1 = dim - 1;

	std::vector<Vertex> adjacent;
	if (y > 0)
		adjacent.push_back({ x, y - 1, dim });

	if (y < dimm1)
		adjacent.push_back({ x, y + 1, dim });

	if (x > 0)
		adjacent.push_back({ x - 1, y, dim });

	if (x < dimm1)
		adjacent.push_back({ x + 1, y, dim });

	for(auto & v: adjacent) {
		auto c = cm.getAt(v);

		if (c == nullptr || (c->type == for_whom && c->liberties.size() > 1) || (c->type != for_whom && c->liberties.size() == 1))
			target->insert(v);
	}
}

bool checkLiberty(const ChainMap & cm, const int x, const int y, const board_t for_whom)
{
	bool      ok    = false;
	const int dim   = cm.getDim();
	const int dimm1 = dim - 1;

	std::vector<chain_t *> crosses;

	if (x > 0)
		crosses.push_back(cm.getAt({ x - 1, y, dim }));

	if (x < dimm1)
		crosses.push_back(cm.getAt({ x + 1, y, dim }));

	if (y > 0)
		crosses.push_back(cm.getAt({ x, y - 1, dim }));

	if (y < dimm1)
		crosses.push_back(cm.getAt({ x, y + 1, dim }));

	for(auto & c: crosses) {
		if (c == nullptr || (c->type == for_whom && c->liberties.size() > 1) || (c->type != for_whom && c->liberties.size() == 1)) {
			ok = true;
			break;
		}
	}

	return ok;
}

void findLiberties(const ChainMap & cm, std::vector<Vertex> *const empties, const board_t for_whom)
{
	const int dim = cm.getDim();

	for(int y=0; y<dim; y++) {
		for(int x=0; x<dim; x++) {
			if (cm.getAt(x, y) != nullptr)
				continue;

			if (checkLiberty(cm, x, y, for_whom))
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

void eraseLiberty(std::vector<Vertex> *const liberties, const Vertex & v)
{
	const size_t n_liberties = liberties->size();

	for(size_t i=0; i<n_liberties; i++) {
		if (liberties->at(i) == v) {
			liberties->at(i) = std::move(liberties->back());
			liberties->pop_back();
			break;
		}
	}
}

void connect(Board *const b, ChainMap *const cm, std::vector<chain_t *> *const chainsWhite, std::vector<chain_t *> *const chainsBlack, std::vector<Vertex> *const libertiesWhite, std::vector<Vertex> *const libertiesBlack, const board_t what, const int x, const int y)
{
	const int dim   = b->getDim();
	const int dimm1 = dim - 1;

	assert(x >= 0 && x < dim);
	assert(y >= 0 && y < dim);

	Vertex v(x, y, dim);

	// update board
	assert(b->getAt(x, y) == B_EMPTY);
	b->setAt(v, what);

	// update global liberties
	eraseLiberty(libertiesWhite, v);

	eraseLiberty(libertiesBlack, v);

	// determine the adjacent fields
	std::vector<Vertex> adjacent;
	if (y > 0)
		adjacent.push_back({ x, y - 1, dim });

	if (y < dimm1)
		adjacent.push_back({ x, y + 1, dim });

	if (x > 0)
		adjacent.push_back({ x - 1, y, dim });

	if (x < dimm1)
		adjacent.push_back({ x + 1, y, dim });

	// find chains to merge
	// also remove the cross underneath the new stone of all chain-liberties
	std::set<chain_t *> toMergeTemp;

	for(auto & vScan : adjacent) {
		auto p = cm->getAt(vScan);

		if (p) {
			p->liberties.erase(v);

			if (p->type == what)
				toMergeTemp.insert(p);
		}
	}

	std::vector<chain_t *> toMerge;
	for(auto & chain : toMergeTemp)
		toMerge.push_back(chain);

	bool rescanGlobalLiberties = false;

	// add new piece to (existing) first chain (of the set of chains found to be merged)
	if (toMerge.empty() == false) {
		// add to chain
		toMerge.at(0)->chain.push_back(v);
		// update board->chain map
		cm->setAt(v, toMerge.at(0));

		// merge
		auto cleanChainSet = what == B_WHITE ? chainsWhite : chainsBlack;

		for(size_t i=1; i<toMerge.size(); i++) {
			toMerge.at(0)->chain.insert(toMerge.at(0)->chain.end(), std::make_move_iterator(toMerge.at(i)->chain.begin()), std::make_move_iterator(toMerge.at(i)->chain.end()));  // add stones

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
		checkLiberty(*cm, x, y, what, &toMerge.at(0)->liberties);
	}
	else {
		// this is a new chain
		chain_t *curChain = new chain_t;
		curChain->type = what;
		curChain->chain.push_back(v);

		if (what == B_WHITE)
			chainsWhite->push_back(curChain);
		else // if (what == B_BLACK)
			chainsBlack->push_back(curChain);

		cm->setAt(v, curChain);

		// find any liberties around it
		checkLiberty(*cm, x, y, what, &curChain->liberties);
	}

	// find surrounding opponent chains of the current position to remove them
	// if they're now dead
	for(auto & vScan : adjacent) {
		auto p = cm->getAt(vScan);

		if (!p || p->liberties.empty() == false || p->type == what)
			continue;

		for(auto ve : p->chain) {
			b->setAt(ve, B_EMPTY);

			cm->setAt(ve, nullptr);

			const int x = ve.getX();
			const int y = ve.getY();

			if (x) {
				auto p = cm->getAt(x - 1, y);
				if (p) {
					p->liberties.insert(ve);
					libertiesWhite->push_back(ve);
					libertiesBlack->push_back(ve);
				}
			}

			if (x < dimm1) {
				auto p = cm->getAt(x + 1, y);
				if (p) {
					p->liberties.insert(ve);
					libertiesWhite->push_back(ve);
					libertiesBlack->push_back(ve);
				}
			}

			if (y) {
				auto p = cm->getAt(x, y - 1);
				if (p) {
					p->liberties.insert(ve);
					libertiesWhite->push_back(ve);
					libertiesBlack->push_back(ve);
				}
			}

			if (y < dimm1) {
				auto p = cm->getAt(x, y + 1);
				if (p) {
					p->liberties.insert(ve);
					libertiesWhite->push_back(ve);
					libertiesBlack->push_back(ve);
				}
			}
		}

		if (p->type == B_WHITE)
			chainsWhite->erase(std::find(chainsWhite->begin(), chainsWhite->end(), p));
		else
			chainsBlack->erase(std::find(chainsBlack->begin(), chainsBlack->end(), p));

		delete p;
	}

#if 0
	if (rescanGlobalLiberties) {
		libertiesWhite->clear();
		findLiberties(*cm, libertiesWhite, B_WHITE);

		libertiesBlack->clear();
		findLiberties(*cm, libertiesBlack, B_BLACK);
	}
#endif
}

void purgeChainsWithoutLiberties(Board *const b, const std::vector<chain_t *> & chains)
{
	for(auto chain : chains) {
		if (chain->liberties.empty()) {
			for(auto ve : chain->chain)
				b->setAt(ve, B_EMPTY);
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

	purgeChainsWithoutLiberties(b, scan);

	purgeChains(&chainsBlack);
	purgeChains(&chainsWhite);
}
