#include <cassert>
#include <cstring>
#include <ranges>

#include "board.h"
#include "str.h"


const char *board_t_name(const board_t v)
{
	if (v == board_t::B_BLACK)
		return "x";

	if (v == board_t::B_WHITE)
		return "o";

	if (v == board_t::B_EMPTY)
		return ".";

	assert(false);

	return "?";
}

uint64_t Board::getHashForField(const int v)
{
	board_t stone = b[v];

	if (stone == board_t::B_EMPTY)
		return 0;

	return z->get(v, stone == board_t::B_BLACK);
}

std::pair<chain *, chain_nr_t> Board::getChain(const Vertex & v)
{
	const int o = v.getV();

	// get chain number from map
	chain_nr_t nr = cm[o];

	if (nr == 0)
		return { nullptr, nr };

	board_t  bv = b[o];

	auto it = bv == board_t::B_BLACK ? blackChains.find(nr) : whiteChains.find(nr);

	assert(it != (bv == board_t::B_BLACK ? blackChains.end() : whiteChains.end()));

	return { it->second, nr };
}

std::pair<chain *, chain_nr_t> Board::getChain(const chain_nr_t nr, const board_t bv)
{
	auto it = bv == board_t::B_BLACK ? blackChains.find(nr) : whiteChains.find(nr);

	assert(it != (bv == board_t::B_BLACK ? blackChains.end() : whiteChains.end()));

	return { it->second, nr };
}

std::pair<chain *, chain_nr_t> Board::getChainConst(const Vertex & v) const
{
	const int o = v.getV();

	chain_nr_t nr = cm[o];

	if (nr == 0)
		return { nullptr, nr };

	auto it = b[o] == board_t::B_BLACK ? blackChains.find(nr) : whiteChains.find(nr);

	assert(it != (b[o] == board_t::B_BLACK ? blackChains.end() : whiteChains.end()));

	return { it->second, nr };
}

void Board::addLiberties(chain *const c, const Vertex & v, std::vector<std::tuple<chain_nr_t, Vertex, bool> > *const undo, const uint64_t cnr)
{
	Vertex vLeft(v.left());
	if (vLeft.isValid() && getAt(vLeft) == board_t::B_EMPTY) {
		c->addLiberty(vLeft);

		undo->push_back({ cnr, vLeft, true });
	}

	Vertex vRight(v.right());
	if (vRight.isValid() && getAt(vRight) == board_t::B_EMPTY) {
		c->addLiberty(vRight);

		undo->push_back({ cnr, vRight, true });
	}

	Vertex vUp(v.up());
	if (vUp.isValid() && getAt(vUp) == board_t::B_EMPTY) {
		c->addLiberty(vUp);

		undo->push_back({ cnr, vUp, true });
	}

	Vertex vDown(v.down());
	if (vDown.isValid() && getAt(vDown) == board_t::B_EMPTY) {
		c->addLiberty(vDown);

		undo->push_back({ cnr, vDown, true });
	}
}

void Board::updateField(const Vertex & v, const board_t bv)
{
	printf(" * updateField(%s, %s) started *\n", v.to_str().c_str(), board_t_name(bv));

//	dump();

	assert(v.isValid());
	assert(v.getDim() == dim);

	assert(b_undo.back().finished == false);

	const int place = v.getV();

	// update layout-undo
	assert(b_undo.back().finished == false);
	b_undo.back().undos.emplace_back(v, b[place], hash);

	// put stone & update
	if (b[place] != board_t::B_EMPTY)
		hash ^= z->get(place, b[place] == board_t::B_BLACK);

	assert((b[place] == board_t::B_EMPTY && (bv == board_t::B_WHITE || bv == board_t::B_BLACK)) || ((b[place] == board_t::B_WHITE || b[place] == board_t::B_BLACK) && bv == board_t::B_EMPTY));
	b[place] = bv;

	if (bv != board_t::B_EMPTY)
		hash ^= z->get(place, bv == board_t::B_BLACK);

	// any chains surrounding this vertex?
	std::unordered_set<Vertex, Vertex::HashFunction> adjacentBlack;
	std::unordered_set<Vertex, Vertex::HashFunction> adjacentWhite;

	Vertex vLeft(v.left());
	if (vLeft.isValid()) {
		board_t bv = getAt(vLeft);

		if (bv == board_t::B_BLACK)
			adjacentBlack.insert(vLeft);
		else if (bv == board_t::B_WHITE)
			adjacentWhite.insert(vLeft);
	}

	Vertex vRight(v.right());
	if (vRight.isValid()) {
		board_t bv = getAt(vRight);

		if (bv == board_t::B_BLACK)  // mine
			adjacentBlack.insert(vRight);
		else if (bv == board_t::B_WHITE)
			adjacentWhite.insert(vRight);
	}

	Vertex vUp(v.up());
	if (vUp.isValid()) {
		board_t bv = getAt(vUp);

		if (bv == board_t::B_BLACK)  // mine
			adjacentBlack.insert(vUp);
		else if (bv == board_t::B_WHITE)
			adjacentWhite.insert(vUp);
	}

	Vertex vDown(v.down());
	if (vDown.isValid()) {
		board_t bv = getAt(vDown);

		if (bv == board_t::B_BLACK)  // mine
			adjacentBlack.insert(vDown);
		else if (bv == board_t::B_WHITE)
			adjacentWhite.insert(vDown);
	}

	// connect/delete chains when placing a stone
	auto & adjacentMine   = bv == board_t::B_BLACK ? adjacentBlack : adjacentWhite;
	auto & adjacentTheirs = bv == board_t::B_BLACK ? adjacentWhite : adjacentBlack;

	// connect new stone to existing chain
	if (adjacentMine.size() == 1) {
		// get chain to connect to
		auto ch = getChain(*adjacentMine.begin());
		printf("connect new stone to chain nr %lu\n", ch.second);
		assert(ch.first  != nullptr);
		assert(ch.second != 0);
		assert(ch.first->getStones()->empty() == false);

		// as this stone connects to the (existing) chain, a liberty
		// is removed (and new ones added later on)
		ch.first->removeLiberty(v);

		c_undo.back().undos_liberties.push_back({ ch.second, v, false });  // remove liberty

		// connect the new stone to the chain it is adjacent to
		ch.first->addStone(v);

		// update chainmap for the new stone to the chain it is connected to
		cm[v.getV()] = ch.second;

		// only 'v' needs to be removed
		c_undo.back().undos.push_back({ ch.second, c_undo_t::modify_t::A_MODIFY, bv, { v } });

		// add liberties of this new stone to the (existing) chain
		addLiberties(ch.first, v, &c_undo.back().undos_liberties, ch.second);
	}
	// connect adjacent chains
	else if (adjacentMine.empty() == false) {
		printf("connect adjacent %zu chains\n", adjacentMine.size());
		// create new chain for adjacent chains
		chain *new_c = new chain();

		printf("new chain: %ld\n", cnr);

		for(auto & ac: adjacentMine) {
			// get pointer of old chain
			auto       ch     = getChain(ac);
			chain     *old_c  = ch.first;
			chain_nr_t old_nr = ch.second;  // get chain number of old chain
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
				c_undo.back().undos_liberties.push_back({ old_nr, liberty, false });  // remove from old chain
				c_undo.back().undos_liberties.push_back({ cnr,    liberty, true  });  // add to new chain
			}

			// register a chain deletion
			c_undo.back().undos.push_back({ old_nr, c_undo_t::modify_t::A_REMOVE, bv, std::move(*old_c->getStones()) });

			printf("delete chain %ld\n", old_nr);

			// delete chain from lists
			bool rc = false;
			if (bv == board_t::B_BLACK)
				rc = blackChains.erase(old_nr);
			else
				rc = whiteChains.erase(old_nr);
			assert(rc);

			delete old_c;
		}

		// remove the liberty where the new stone is placed
		new_c->removeLiberty(v);
		c_undo.back().undos_liberties.push_back({ cnr, v, false });

		// merge the new stone
		new_c->addStone(v);  // add to (new) chain
		cm[place] = cnr;  // put (new) chain in chainmap
		addLiberties(new_c, v, &c_undo.back().undos_liberties, cnr);  // add liberties of this new stone to the (new) chain

		// place the new chain
		bool rc = false;
		if (bv == board_t::B_BLACK)
			rc = blackChains.insert({ cnr, new_c }).second;
		else
			rc = whiteChains.insert({ cnr, new_c }).second;
		assert(rc);

		// register the new chain
		c_undo.back().undos.push_back({ cnr, c_undo_t::modify_t::A_ADD, bv, { } });

		cnr++;
	}
	else {  // new chain
		printf("new chain\n");
		chain *c = new chain();
		// add the new stone in the new chain
		c->addStone(v);

		// put in chainmap
		cm[v.getV()] = cnr;

		// register new chain in the maps
		bool rc = false;
		if (bv == board_t::B_BLACK)
			rc = blackChains.insert({ cnr, c }).second;
		else
			rc = whiteChains.insert({ cnr, c }).second;
		assert(rc);

		// register that a new chain was registered
		c_undo.back().undos.push_back({ cnr, c_undo_t::modify_t::A_ADD, bv, { } });

		// give the chain its liberties
		addLiberties(c, v, &c_undo.back().undos_liberties, cnr);

		cnr++;
	}

	// remove this liberty from adjacent chains 
	for(auto & ac: adjacentTheirs) {
		auto ch = getChain(ac);

		ch.first->removeLiberty(v);

		c_undo.back().undos_liberties.push_back({ ch.second, ac, false });
	}

	// check if any surrounding chains are dead
	for(auto & ac: adjacentTheirs) {
		auto       ch      = getChain(ac);
		chain     *work_c  = ch.first;
		chain_nr_t work_nr = ch.second;
		board_t    work_b  = bv == board_t::B_BLACK ? board_t::B_WHITE : board_t::B_BLACK;

		if (work_c->isDead() == false)
			continue;

		printf("delete dead chain\n");

		// clean-up
		for(auto & stone : *work_c->getStones()) {
			assert(b[stone.getV()] == work_b);

			// update undo record
			b_undo.back().undos.emplace_back(stone, b[stone.getV()], hash);

			// remove stone from board
			b[stone.getV()] = board_t::B_EMPTY;

			// update hash
			hash ^= z->get(stone.getV(), bv == board_t::B_BLACK);

			// remove stone from chainmap
			cm[stone.getV()] = 0;

			// register new liberties in surrounding chains
			Vertex vLeft(stone.left());
			if (vLeft.isValid()) {
				auto   ch    = getChain(vLeft);
				chain *old_c = ch.first;

				if (old_c) {
					old_c->addLiberty(stone);

					c_undo.back().undos_liberties.push_back({ ch.second, stone, true });
				}
			}

			Vertex vRight(stone.right());
			if (vRight.isValid()) {
				auto   ch    = getChain(vRight);
				chain *old_c = ch.first;

				if (old_c) {
					old_c->addLiberty(stone);

					c_undo.back().undos_liberties.push_back({ ch.second, stone, true });
				}
			}

			Vertex vUp(stone.up());
			if (vUp.isValid()) {
				auto   ch    = getChain(vUp);
				chain *old_c = ch.first;

				if (old_c) {
					old_c->addLiberty(stone);

					c_undo.back().undos_liberties.push_back({ ch.second, stone, true });
				}
			}

			Vertex vDown(stone.down());
			if (vDown.isValid()) {
				auto   ch    = getChain(vDown);
				chain *old_c = ch.first;

				if (old_c) {
					old_c->addLiberty(stone);

					c_undo.back().undos_liberties.push_back({ ch.second, stone, true });
				}
			}
		}

		// register a chain deletion
		c_undo.back().undos.push_back({ work_nr, c_undo_t::modify_t::A_REMOVE, bv == board_t::B_BLACK ? board_t::B_WHITE : board_t::B_BLACK, std::move(*work_c->getStones()) });

		// remove chain from lists
		bool rc = false;
		if (bv == board_t::B_WHITE)  // !we're purging opponent chains!
			rc = blackChains.erase(work_nr);
		else
			rc = whiteChains.erase(work_nr);
		assert(rc);
	}

	dump();

	printf("updateField(%s) finishes with hash %016lx\n", v.to_str().c_str(), hash);
}

uint64_t Board::getHashForMove(const int v, const board_t bv)
{
	uint64_t out = hash;

	if (b[v] != board_t::B_EMPTY)
		out ^= z->get(v, b[v] == board_t::B_BLACK);

	if (bv != board_t::B_EMPTY)
		out ^= z->get(v, bv == board_t::B_BLACK);

	return out;
}

Board::Board(Zobrist *const z, const int dim) : z(z), dim(dim), b(new board_t[dim * dim]()), cm(new chain_nr_t[dim * dim]())
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

	cm = new chain_nr_t[dimsq]();

	z->setDim(dim);

	int str_o = 0;

	for(int y=dim - 1; y >= 0; y--) {
		for(int x=0; x<dim; x++) {
			char c = str[str_o];

			if (c == 'w' || c == 'W')
				putAt(x, y, board_t::B_WHITE);
			else if (c == 'b' || c == 'B')
				putAt(x, y, board_t::B_BLACK);
			else
				assert(c == '.');

			str_o++;
		}

		str_o++;  // skip slash
	}
}

Board::~Board()
{
	delete [] b;

	delete [] cm;

	for(auto & element: blackChains)
		delete element.second;

	for(auto & element: whiteChains)
		delete element.second;
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

	printf("finish setup with %s\n", dumpFEN(board_t::B_BLACK, 0).c_str());
}

size_t Board::getUndoDepth()
{
	assert(b_undo.size() == c_undo.size());

	return c_undo.size();
}

void Board::undoMoveSet()
{
	printf("-------------------------------- * UNDO with hash %016lx *\n", hash);
	printf("*** UNDO INPUT:\n");
	dump();
	// undo stones
	// mostly used for dead chains, not when e.g. replacing chains
	assert(b_undo.back().finished == true);

	b_undo.back().dump();

	for(auto & tuple: std::ranges::views::reverse(b_undo.back().undos)) {
		Vertex  & v = std::get<0>(tuple);
		board_t   t = std::get<1>(tuple);

		b[v.getV()] = t;

		hash        = std::get<2>(tuple);
	}

	b_undo.pop_back();

	// undo chains
	assert(c_undo.back().finished == true);

	c_undo.back().dump();

	for(auto & tuple: std::ranges::views::reverse(c_undo.back().undos)) {
		chain_nr_t         nr     = std::get<0>(tuple);
		c_undo_t::modify_t action = std::get<1>(tuple);
		board_t            col    = std::get<2>(tuple);

		if (action == c_undo_t::modify_t::A_ADD) {  // chain was added? then remove it now
			auto c = getChain(nr, col);

			bool rc = false;
			if (col == board_t::B_BLACK)
				rc = blackChains.erase(nr);
			else
				rc = whiteChains.erase(nr);
			assert(rc);

			for(auto & stone: *c.first->getStones()) {
				assert(cm[stone.getV()] != 0);
				cm[stone.getV()] = 0;
			}

			delete c.first;
		}
		else if (action == c_undo_t::modify_t::A_REMOVE) {
			chain *c = new chain();

			// re-register the re-created old chain
			bool rc = false;
			if (col == board_t::B_BLACK)
				rc = blackChains.insert({ nr, c }).second;
			else
				rc = whiteChains.insert({ nr, c }).second;
			assert(rc);

			// add the stones and update chainmap
			for(auto & stone: std::get<3>(tuple)) {
				assert(cm[stone.getV()] == 0);
				cm[stone.getV()] = nr;

				assert(getAt(stone.getV()) == col);
			}
		}
		else if (action == c_undo_t::modify_t::A_MODIFY) {
			auto c = getChain(nr, col);

			for(auto & stone: std::get<3>(tuple))
				c.first->removeStone(stone);
		}
		else {
			assert(0);
		}
	}

	// undo liberties
	for(auto & tuple: std::ranges::views::reverse(c_undo.back().undos_liberties)) {
		Vertex & v   = std::get<1>(tuple);
		bool     add = std::get<2>(tuple);

		auto     c   = getChain(v);
		printf("undo liberty: %s was %s, chain %ld\n", add ? "remove" : "add", v.to_str().c_str(), c.second);

		// a chain may already have been purged earlier in this method
		if (c.second == 0)
			continue;

		if (add)  // liberty was added? then remove it now
			c.first->removeLiberty(v);
		else
			c.first->addLiberty(v);
	}

	c_undo.pop_back();
	printf("*** UNDO OUTPUT:\n");
	dump();
	printf("* UNDO with hash %016lx finished * --------------------------------\n", hash);
}

void Board::putAt(const Vertex & v, const board_t bv)
{
	assert(bv == board_t::B_BLACK || bv == board_t::B_WHITE);

	updateField(v, bv);
}

void Board::putAt(const int x, const int y, const board_t bv)
{
	assert(bv == board_t::B_BLACK || bv == board_t::B_WHITE);

	updateField({ x, y, dim }, bv);
}

std::vector<Vertex> * Board::findLiberties(const board_t for_whom)
{
	std::vector<Vertex> *empties = new std::vector<Vertex>;

	const int dimsq = dim * dim;
	const int dimm1 = dim - 1;

	bool *okFields = new bool[dimsq];

	for(int i=0; i<dimsq; i++) {
		board_t bv = getAt(i);

		chain *c = nullptr;

		if (bv != board_t::B_EMPTY)
			c = (bv == board_t::B_BLACK ? &blackChains : &whiteChains)->find(cm[i])->second;

		okFields[i] = c == nullptr || (bv == for_whom && c->getLiberties()->size() > 1) || (bv != for_whom && c->getLiberties()->size() == 1);
	}

	empties->reserve(dimsq);

	int o = 0;

	for(int y=0; y<dim; y++) {
		for(int x=0; x<dim; x++, o++) {
			if (getAt(o) != board_t::B_EMPTY)
				continue;

			if ((x > 0 && okFields[o - 1]) || (x < dimm1 && okFields[o + 1]) || (y > 0 && okFields[o - dim]) || (y < dimm1 && okFields[o + dim]))
				empties->emplace_back(o, dim);
		}
	}

	delete [] okFields;

	return empties;
}

void Board::dump()
{
	printf("black chains:\n");
	for(auto & chain: blackChains)
		chain.second->dump();

	printf("white chains:\n");
	for(auto & chain: whiteChains)
		chain.second->dump();

        std::string line;

        for(int y=dim - 1; y>=0; y--) {
                line = myformat("# %2d | ", y + 1);

		// which stone
                for(int x=0; x<dim; x++) {
                        board_t bv = getAt(x, y);

                        if (bv == board_t::B_EMPTY)
                                line += ".";
                        else if (bv == board_t::B_BLACK)
                                line += "x";
                        else if (bv == board_t::B_WHITE)
                                line += "o";
                        else
                                line += "!";
                }

		line += "   ";

		// chain number
                for(int x=0; x<dim; x++) {
			auto chain = getChainConst(Vertex(x, y, dim));

			line += myformat("%5d", chain.second);
                }

		line += "   ";

		// liberty count
                for(int x=0; x<dim; x++) {
			auto chain = getChainConst(Vertex(x, y, dim));

			if (chain.first)
				line += myformat(" %zu", chain.first->getLiberties()->size());
			else
				line += " -";
                }

                printf("%s\n", line.c_str());
        }

        line = "#      ";

        for(int x=0; x<dim; x++) {
                int xc = 'A' + x;

                if (xc >= 'I')
                        xc++;

                line += myformat("%c", xc);
        }

        printf("%s\n", line.c_str());
}

std::string Board::dumpFEN(const board_t next_player, const int pass_depth)
{
        std::string out;

        for(int y=dim - 1; y >= 0; y--) {
                for(int x=0; x<dim; x++) {
                        auto stone = getAt(x, y);

                        if (stone == board_t::B_EMPTY)
                                out += ".";
                        else if (stone == board_t::B_BLACK)
                                out += "b";
                        else
                                out += "w";
                }

                if (y)
                        out += "/";
        }

        out += next_player == board_t::B_WHITE ? " w" : " b";

        out += myformat(" %d", pass_depth);

        return out;
}
