#include <cassert>
#include <cstring>
#include <ranges>

#include "board.h"


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

std::pair<chain *, uint64_t> Board::getChain(const Vertex & v)
{
	const int o = v.getV();

	// get chain number from map
	uint64_t nr = cm[o];

	if (nr == 0)
		return { nullptr, nr };

	board_t  bv = b[o];

	auto it = bv == B_BLACK ? blackChains.find(nr) : whiteChains.find(nr);

	assert(it != (bv == B_BLACK ? blackChains.end() : whiteChains.end()));

	return { it->second, nr };
}

std::pair<chain *, uint64_t> Board::getChainConst(const Vertex & v) const
{
	const int o = v.getV();

	uint64_t nr = cm[o];

	if (nr == 0)
		return { nullptr, nr };

	auto it = b[o] == B_BLACK ? blackChains.find(nr) : whiteChains.find(nr);

	assert(it != (b[o] == B_BLACK ? blackChains.end() : whiteChains.end()));

	return { it->second, nr };
}

void Board::addLiberties(chain *const c, const Vertex & v)
{
	Vertex vLeft(v.left());
	if (vLeft.isValid() && getAt(vLeft) == B_EMPTY)
		c->addLiberty(vLeft);

	Vertex vRight(v.right());
	if (vRight.isValid() && getAt(vRight) == B_EMPTY)
		c->addLiberty(vRight);

	Vertex vUp(v.up());
	if (vUp.isValid() && getAt(vUp) == B_EMPTY)
		c->addLiberty(vUp);

	Vertex vDown(v.down());
	if (vDown.isValid() && getAt(vDown) == B_EMPTY)
		c->addLiberty(vDown);
}

void Board::updateField(const Vertex & v, const board_t bv)
{
	assert(v.isValid());
	assert(v.getDim() == dim);

	assert(b_undo.back().finished == false);

	const int place = v.getV();

	// update layout-undo
	assert(b_undo.back().finished == false);
	b_undo.back().undos.emplace_back(v, b[place], hash);

	// put stone & update
	if (b[place] != B_EMPTY)
		hash ^= z->get(place, b[place] == B_BLACK);

	b[place] = bv;

	if (bv != B_EMPTY)
		hash ^= z->get(place, bv == B_BLACK);

	// any chains surrounding this vertex?
	std::unordered_set<Vertex, Vertex::HashFunction> adjacentBlack;
	std::unordered_set<Vertex, Vertex::HashFunction> adjacentWhite;

	Vertex vLeft(v.left());
	if (vLeft.isValid()) {
		board_t bv = getAt(vLeft);

		if (bv == B_BLACK)
			adjacentBlack.insert(vLeft);
		else if (bv == B_WHITE)
			adjacentWhite.insert(vLeft);
	}

	Vertex vRight(v.right());
	if (vRight.isValid()) {
		board_t bv = getAt(vRight);

		if (bv == B_BLACK)  // mine
			adjacentBlack.insert(vRight);
		else if (bv == B_WHITE)
			adjacentWhite.insert(vRight);
	}

	Vertex vUp(v.up());
	if (vUp.isValid()) {
		board_t bv = getAt(vUp);

		if (bv == B_BLACK)  // mine
			adjacentBlack.insert(vUp);
		else if (bv == B_WHITE)
			adjacentWhite.insert(vUp);
	}

	Vertex vDown(v.down());
	if (vDown.isValid()) {
		board_t bv = getAt(vDown);

		if (bv == B_BLACK)  // mine
			adjacentBlack.insert(vDown);
		else if (bv == B_WHITE)
			adjacentWhite.insert(vDown);
	}

	// connect/delete chains when placing a stone
	auto & adjacentMine   = bv == B_BLACK ? adjacentBlack : adjacentWhite;
	auto & adjacentTheirs = bv == B_BLACK ? adjacentWhite : adjacentBlack;

	// connect adjacent chains
	if (adjacentMine.empty() == false) {
		// create new chain for adjacent chains
		chain *new_c = new chain();

		for(auto & ac: adjacentMine) {
			// get pointer of old chain
			auto     ch     = getChain(ac);
			chain   *old_c  = ch.first;
			uint64_t old_nr = ch.second;  // get chain number of old chain
			assert(old_nr != cnr);

			// add stones of the to-connect-chains to the new chain
			for(auto & stone: *old_c->getStones()) {
				new_c->addStone(stone);

				// also update the chain map; make the old stones point
				// to the new chain
				cm[stone.getV()] = cnr;
			}

			// copy the liberties
			for(auto & liberty: *old_c->getLiberties()) {
				new_c->addLiberty(liberty);

				// undo-management
				c_undo.back().undos_liberties.push_back({ old_c, liberty, false });  // remove from old chain
				c_undo.back().undos_liberties.push_back({ new_c, liberty, true  });  // add to new chain
			}

			// register a chain deletion
			c_undo.back().undos.push_back({ old_nr, old_c, false, bv });

			// delete chain from lists
			bool rc = false;
			if (bv == B_BLACK)
				rc = blackChains.erase(old_nr);
			else
				rc = whiteChains.erase(old_nr);
			assert(rc);
		}

		// remove the liberty where the new stone is placed
		new_c->removeLiberty(v);
		c_undo.back().undos_liberties.push_back({ new_c, v, false });

		// merge the new stone
		new_c->addStone(v);  // add to (new) chain
		cm[place] = cnr;  // put (new) chain in chainmap
		addLiberties(new_c, v);  // add liberties of this new stone to the (new) chain

		// place the new chain
		bool rc = false;
		if (bv == B_BLACK)
			rc = blackChains.insert({ cnr, new_c }).second;
		else
			rc = whiteChains.insert({ cnr, new_c }).second;
		assert(rc);

		// register the new chain
		c_undo.back().undos.push_back({ cnr, new_c, true, bv });

		cnr++;
	}
	else {  // new chain
		chain *c = new chain();
		// add the new stone in the new chain
		c->addStone(v);

		// put in chainmap
		cm[v.getV()] = cnr;

		// register new chain in the maps
		bool rc = false;
		if (bv == B_BLACK)
			rc = blackChains.insert({ cnr, c }).second;
		else
			rc = whiteChains.insert({ cnr, c }).second;
		assert(rc);

		// register that a new chain was registered
		c_undo.back().undos.push_back({ cnr, c, true, bv });

		// give the chain its liberties
		addLiberties(c, v);

		// remove this liberty from adjacent chains 
		for(auto & ac: adjacentTheirs) {
			auto ch = getChain(ac);

			ch.first->removeLiberty(v);

			c_undo.back().undos_liberties.push_back({ ch.first, ac, false });
		}

		cnr++;
	}

	// check if any surrounding chains are dead
	for(auto & ac: adjacentTheirs) {
		auto     ch      = getChain(ac);
		chain   *work_c  = ch.first;
		uint64_t work_nr = ch.second;
		board_t  work_b  = bv == B_BLACK ? B_WHITE : B_BLACK;

		if (work_c->isDead() == false)
			continue;

		// clean-up
		for(auto & stone : *work_c->getStones()) {
			assert(b[stone.getV()] == work_b);

			// update undo record
			b_undo.back().undos.emplace_back(stone, b[stone.getV()], hash);

			// remove stone from board
			b[stone.getV()] = B_EMPTY;

			// update hash
			hash ^= z->get(stone.getV(), bv == B_BLACK);

			// remove stone from chainmap
			cm[stone.getV()] = 0;

			// register new liberties in surrounding chains
			Vertex vLeft(v.left());
			if (vLeft.isValid()) {
				auto   ch    = getChain(vLeft);
				chain *old_c = ch.first;

				if (old_c) {
					old_c->addLiberty(stone);

					c_undo.back().undos_liberties.push_back({ old_c, vLeft, true });
				}
			}

			Vertex vRight(v.right());
			if (vRight.isValid()) {
				auto   ch    = getChain(vRight);
				chain *old_c = ch.first;

				if (old_c) {
					old_c->addLiberty(stone);

					c_undo.back().undos_liberties.push_back({ old_c, vRight, true });
				}
			}

			Vertex vUp(v.up());
			if (vUp.isValid()) {
				auto   ch    = getChain(vUp);
				chain *old_c = ch.first;

				if (old_c) {
					old_c->addLiberty(stone);

					c_undo.back().undos_liberties.push_back({ old_c, vUp, true });
				}
			}

			Vertex vDown(v.down());
			if (vDown.isValid()) {
				auto   ch    = getChain(vDown);
				chain *old_c = ch.first;

				if (old_c) {
					old_c->addLiberty(stone);

					c_undo.back().undos_liberties.push_back({ old_c, vDown, true });
				}
			}
		}

		// register a chain deletion
		c_undo.back().undos.push_back({ work_nr, work_c, false, bv == B_BLACK ? B_WHITE : B_BLACK });

		// remove chain from lists
		bool rc = false;
		if (bv == B_WHITE)  // !we're purging opponent chains!
			rc = blackChains.erase(work_nr);
		else
			rc = whiteChains.erase(work_nr);
		assert(rc);
	}
}

uint64_t Board::getHashForMove(const int v, const board_t bv)
{
	uint64_t out = hash;

	if (b[v] != B_EMPTY)
		out ^= z->get(v, b[v] == B_BLACK);

	if (bv != B_EMPTY)
		out ^= z->get(v, bv == B_BLACK);

	return out;
}

Board::Board(Zobrist *const z, const int dim) : z(z), dim(dim), b(new board_t[dim * dim]()), cm(new uint64_t[dim * dim]())
{
	assert(dim & 1);

	z->setDim(dim);
}

Board::Board(Zobrist *const z, const std::string & str) : z(z)
{
	auto slash = str.find('/');

	dim = slash;

	int dimsq = dim * dim;

	b = new board_t[dimsq]();

	cm = new uint64_t[dimsq]();

	z->setDim(dim);

	int str_o = 0;

	for(int y=dim - 1; y >= 0; y--) {
		for(int x=0; x<dim; x++) {
			char c = str[str_o];

			if (c == 'w' || c == 'W')
				putAt(x, y, B_WHITE);
			else if (c == 'b' || c == 'B')
				putAt(x, y, B_BLACK);
			else
				assert(c == '.');

			str_o++;
		}

		str_o++;  // skip slash
	}
}

Board::Board(const Board & bIn) : z(bIn.z), dim(bIn.getDim()), b(new board_t[dim * dim]), cm(new uint64_t[dim * dim ]())
{
	assert(dim & 1);

	bIn.getTo(b);

	// TODO: copy chains

	hash = bIn.hash;
}

Board::~Board()
{
	delete [] b;

	delete [] cm;

	for(auto & element: blackChains)
		delete element.second;

	for(auto & element: whiteChains)
		delete element.second;

	for(auto & element: c_undo) {
		for(auto & subelement: element.undos)
			if (std::get<2>(subelement) == false)
				delete std::get<1>(subelement);
	}
}

Board & Board::operator=(const Board & in)
{
	dim = in.getDim();

	// copy contents
	in.getTo(b);

	// adjust hash
	hash = in.getHash();

	return *this;
}

// this ignores undo history!
bool Board::operator==(const Board & rhs)
{
	if (getHash() != rhs.getHash())
		return false;

	if (dim != rhs.getDim())
		return false;

	for(int o=0; o<dim * dim; o++) {
		if (getAt(o) != rhs.getAt(o))
			return false;
	}

	return true;
}

bool Board::operator!=(const Board & rhs)
{
	return !(*this == rhs);
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

board_t Board::getAt(const Vertex & v) const
{
	return b[v.getV()];
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

void Board::startMove()
{
	b_undo_t ub;
	b_undo.push_back(ub);

	c_undo_t uc;
	c_undo.push_back(uc);
}

void Board::finishMove()
{
	b_undo.back().finished = true;
	c_undo.back().finished = true;
}

size_t Board::getUndoDepth()
{
	assert(b_undo.size() == c_undo.size());

	return c_undo.size();
}

void Board::undoMoveSet()
{
	// undo stones
	assert(b_undo.back().finished == true);

	// TODO: this can be wrapped into c_undo (e.g. b_undo is not required)
	for(auto & tuple: std::ranges::views::reverse(b_undo.back().undos)) {
		Vertex  & v = std::get<0>(tuple);
		board_t   t = std::get<1>(tuple);

		b[v.getV()] = t;

		hash        = std::get<2>(tuple);
	}

	b_undo.pop_back();

	// undo chains
	assert(c_undo.back().finished == true);

	for(auto & tuple: std::ranges::views::reverse(c_undo.back().undos)) {
		uint64_t  nr  = std::get<0>(tuple);
		chain    *c   = std::get<1>(tuple);
		bool      add = std::get<2>(tuple);
		board_t   col = std::get<3>(tuple);

		if (add) {  // chain was added? then remove it now
			bool rc = false;
			if (col == B_BLACK)
				rc = blackChains.erase(nr);
			else
				rc = whiteChains.erase(nr);
			assert(rc);

			for(auto & stone: *c->getStones()) {
				assert(cm[stone.getV()] != 0);
				cm[stone.getV()] = 0;
				assert(getAt(stone.getV()) == B_EMPTY);
			}

			delete c;
		}
		else {
			bool rc = false;
			if (col == B_BLACK)
				rc = blackChains.insert({ nr, c }).second;
			else
				rc = whiteChains.insert({ nr, c }).second;
			assert(rc);

			for(auto & stone: *c->getStones()) {
				assert(cm[stone.getV()] == 0);
				cm[stone.getV()] = nr;

				assert(getAt(stone.getV()) == col);
			}
		}
	}

	// undo liberties
	for(auto & tuple: std::ranges::views::reverse(c_undo.back().undos_liberties)) {
		chain    *c  = std::get<0>(tuple);
		Vertex & v   = std::get<1>(tuple);
		bool     add = std::get<2>(tuple);

		if (add)  // liberty was added? then remove it now
			c->removeLiberty(v);
		else
			c->addLiberty(v);
	}

	c_undo.pop_back();
}

void Board::putAt(const Vertex & v, const board_t bv)
{
	assert(bv == B_BLACK || bv == B_WHITE);

	updateField(v, bv);
}

void Board::putAt(const int x, const int y, const board_t bv)
{
	assert(bv == B_BLACK || bv == B_WHITE);

	updateField({ x, y, dim }, bv);
}
