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

board_t opponentColor(const board_t v)
{
	if (v == board_t::B_BLACK)
		return board_t::B_WHITE;

	if (v == board_t::B_WHITE)
		return board_t::B_BLACK;

	assert(0);

	return board_t::B_EMPTY;
}

std::pair<chain *, chain_nr_t> Board::getChain(const Vertex & v)
{
	const int o = v.getV();

	// get chain number from map
	chain_nr_t nr = cm[o];

	if (nr == NO_CHAIN)
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

	if (nr == NO_CHAIN)
		return { nullptr, nr };

	auto it = b[o] == board_t::B_BLACK ? blackChains.find(nr) : whiteChains.find(nr);

	assert(it != (b[o] == board_t::B_BLACK ? blackChains.end() : whiteChains.end()));

	return { it->second, nr };
}

void Board::increaseChainNr()
{
	cnr++;

	assert(cnr != NO_CHAIN);
}

auto Board::getLiberties(const Vertex & v)
{
	std::unordered_set<Vertex, Vertex::HashFunction> out;

	Vertex vLeft(v.left());
	if (vLeft.isValid() && getAt(vLeft) == board_t::B_EMPTY)
		out.insert(vLeft);

	Vertex vRight(v.right());
	if (vRight.isValid() && getAt(vRight) == board_t::B_EMPTY)
		out.insert(vRight);

	Vertex vUp(v.up());
	if (vUp.isValid() && getAt(vUp) == board_t::B_EMPTY)
		out.insert(vUp);

	Vertex vDown(v.down());
	if (vDown.isValid() && getAt(vDown) == board_t::B_EMPTY)
		out.insert(vDown);

	return out;
}

auto Board::getSurroundingNonEmptyVertexes(const Vertex & v)
{
	std::unordered_set<Vertex, Vertex::HashFunction> out;

	Vertex vLeft(v.left());
	if (vLeft.isValid() && getAt(vLeft) != board_t::B_EMPTY)
		out.insert(vLeft);

	Vertex vRight(v.right());
	if (vRight.isValid() && getAt(vRight) != board_t::B_EMPTY)
		out.insert(vRight);

	Vertex vUp(v.up());
	if (vUp.isValid() && getAt(vUp) != board_t::B_EMPTY)
		out.insert(vUp);

	Vertex vDown(v.down());
	if (vDown.isValid() && getAt(vDown) != board_t::B_EMPTY)
		out.insert(vDown);

	return out;
}

auto Board::getSurroundingChainsOfType(const Vertex & v, const board_t bv)
{
	std::unordered_set<chain_nr_t> out;

	Vertex vLeft(v.left());
	if (vLeft.isValid() && getAt(vLeft) == bv)
		out.insert(cm[vLeft.getV()]);

	Vertex vRight(v.right());
	if (vRight.isValid() && getAt(vRight) == bv)
		out.insert(cm[vRight.getV()]);

	Vertex vUp(v.up());
	if (vUp.isValid() && getAt(vUp) == bv)
		out.insert(cm[vUp.getV()]);

	Vertex vDown(v.down());
	if (vDown.isValid() && getAt(vDown) == bv)
		out.insert(cm[vDown.getV()]);

	return out;
}

void Board::addChain(const board_t bv, chain_nr_t cnr, chain *const new_c)
{
	auto & chain = bv == board_t::B_BLACK ? blackChains : whiteChains;

	auto rc = chain.insert({ cnr, new_c });

	assert(rc.second);
}

void Board::removeChain(const board_t bv, const chain_nr_t nr)
{
	auto & chain = bv == board_t::B_BLACK ? blackChains : whiteChains;

	bool rc = chain.erase(nr);

	assert(rc);
}

void Board::mapChain(const Vertex & v, const chain_nr_t nr)
{
	assert(cm[v.getV()] == NO_CHAIN);

	cm[v.getV()] = nr;
}

void Board::mapChain(const std::unordered_set<Vertex, Vertex::HashFunction> & chain, const chain_nr_t nr)
{
	for(auto & v: chain)
		mapChain(v, nr);
}

void Board::updateField(const Vertex & v, const board_t bv)
{
	printf(" * updateField(%s, %s) started *\n", v.to_str().c_str(), board_t_name(bv));

//	dump();

	assert(v.isValid());
	assert(v.getDim() == dim);

	const int place = v.getV();

	// update layout-undo
	b_undo.back().undos.emplace_back(v, b[place], hash);
	assert(b[place] == board_t::B_EMPTY);
	assert(bv != board_t::B_EMPTY);

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
		assert(ch.second != NO_CHAIN);
		assert(ch.first->getStones()->empty() == false);

		// as this stone connects to the (existing) chain, a liberty
		// is removed (and new ones added later on)
		ch.first->removeLiberty(v);

		// connect the new stone to the chain it is adjacent to
		ch.first->addStone(v);

		auto new_liberties = getLiberties(v);

		ch.first->addLiberties(new_liberties);

		// update chainmap for the new stone to the chain it is connected to
		mapChain(v, ch.second);

		c_undo_t::action_t action;
		action.nr     = ch.second;  // chain that is modified
		action.action = c_undo_t::modify_t::A_MODIFY;
		action.bv     = bv;  // stone type added
		action.stones = { v };
		action.liberties = new_liberties;  // add liberties of this new stone
		action.chains_lost_liberty = getSurroundingChainsOfType(v, bv == board_t::B_BLACK ? board_t::B_WHITE : board_t::B_BLACK);  // see what chains lost a liberty

		c_undo.back().undos.push_back(std::move(action));
	}
	// connect adjacent chains
	else if (adjacentMine.empty() == false) {
		printf("connect adjacent %zu chains\n", adjacentMine.size());

		auto temp = getChain(*adjacentMine.begin());
		chain     *target_c  = temp.first;
		chain_nr_t target_nr = temp.second;

		for(auto & ac: adjacentMine | std::views::drop(1)) {
			assert(ac != *adjacentMine.begin());

			// get pointer of old chain
			auto       ch     = getChain(ac);
			chain     *old_c  = ch.first;
			chain_nr_t old_nr = ch.second;  // get chain number of old chain
			assert(old_nr != target_nr);

			// add stones of the to-connect-chains to the target chain
			target_c->addStones(*old_c->getStones());

			// copy the liberties
			target_c->addLiberties(*old_c->getLiberties());

			// register the modification
			c_undo_t::action_t action_mod;
			action_mod.nr        = target_nr;
			action_mod.action    = c_undo_t::modify_t::A_MODIFY;
			action_mod.bv        = bv;
			action_mod.stones    = *old_c->getStones();
			action_mod.liberties = *old_c->getLiberties();

			c_undo.back().undos.push_back(std::move(action_mod));

			// register a chain deletion
			c_undo_t::action_t action_rm;
			action_rm.nr        = old_nr;
			action_rm.action    = c_undo_t::modify_t::A_REMOVE;
			action_rm.bv        = bv;  // stone type added
			action_rm.stones    = *old_c->getStones();  // these are removed
			action_rm.liberties = *old_c->getLiberties();

			c_undo.back().undos.push_back(std::move(action_rm));

			// delete chain from lists
			removeChain(bv, old_nr);

			delete old_c;
		}

		// remove the liberty where the new stone is placed
		target_c->removeLiberty(v);

		// merge the new stone
		target_c->addStone(v);  // add to (new) chain

		// adjacent liberties
		auto new_liberties = getLiberties(v);
		target_c->addLiberties(new_liberties);

		mapChain(v, target_nr);

		// register the new chain
		c_undo_t::action_t action_add;
		action_add.nr     = cnr;
		action_add.action = c_undo_t::modify_t::A_MODIFY;
		action_add.bv     = bv;  // stone type added
		action_add.stones = { v };
		action_add.liberties = new_liberties;  // add liberties of this new stone
		action_add.chains_lost_liberty = getSurroundingChainsOfType(v, bv == board_t::B_BLACK ? board_t::B_WHITE : board_t::B_BLACK);  // see what chains lost a liberty

		c_undo.back().undos.push_back(std::move(action_add));

		increaseChainNr();
	}
	else {  // new chain
		printf("new chain\n");
		chain *new_c = new chain();
		// add the new stone in the new chain
		new_c->addStone(v);

		// put in chainmap
		cm[v.getV()] = cnr;

		// register new chain in the maps
		addChain(bv, cnr, new_c);

		// give the chain its liberties
		new_c->addLiberties(getLiberties(v));

		// register that a new chain was registered
		c_undo_t::action_t action;
		action.nr     = cnr;
		action.action = c_undo_t::modify_t::A_ADD;
		action.bv     = bv;  // stone type added
		action.stones = { v };
		action.liberties = *new_c->getLiberties();  // add liberties of this new stone and older chains
		action.chains_lost_liberty = getSurroundingChainsOfType(v, bv == board_t::B_BLACK ? board_t::B_WHITE : board_t::B_BLACK);  // see what chains lost a liberty

		c_undo.back().undos.push_back(std::move(action));

		increaseChainNr();
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

		std::unordered_set<chain_nr_t> liberty_gainers;

		// clean-up
		for(auto & stone : *work_c->getStones()) {
			assert(b[stone.getV()] == work_b);

			assert(v != stone);

			// update undo record
			b_undo.back().undos.emplace_back(stone, b[stone.getV()], hash);
			assert(b[stone.getV()] != board_t::B_EMPTY);

			// remove stone from board
			assert(b[stone.getV()] != board_t::B_EMPTY);
			b[stone.getV()] = board_t::B_EMPTY;

			// update hash to no longer include this stone
			hash ^= z->get(stone.getV(), bv == board_t::B_BLACK);

			// remove stone from chainmap
			mapChain(stone, NO_CHAIN);

			// register new liberties in surrounding chains
			auto adjacentNonEmptyVertexes = getSurroundingNonEmptyVertexes(stone);

			for(auto & adjacentV: adjacentNonEmptyVertexes) {
				auto   ch    = getChain(adjacentV);
				chain *old_c = ch.first;

				old_c->addLiberty(v);

				liberty_gainers.insert(ch.second);
			}
		}

		// register a chain deletion
		c_undo_t::action_t action_rm;
		action_rm.nr     = work_nr;
		action_rm.action = c_undo_t::modify_t::A_REMOVE;
		action_rm.bv     = bv == board_t::B_BLACK ? board_t::B_WHITE : board_t::B_BLACK;
		action_rm.stones = *work_c->getStones();  // these are removed
		action_rm.chains_gained_liberty = liberty_gainers;

		c_undo.back().undos.push_back(std::move(action_rm));

		// remove chain from lists
		removeChain(bv == board_t::B_BLACK ? board_t::B_WHITE : board_t::B_BLACK, work_nr);

		delete work_c;
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
	assert(b_undo.size() == c_undo.size());

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
	for(auto & tuple: b_undo.back().undos) {
		Vertex  & v = std::get<0>(tuple);
		board_t   t = std::get<1>(tuple);

		b[v.getV()] = t;

		hash        = std::get<2>(tuple);
	}

	b_undo.pop_back();

	// undo chains
	for(auto & a: std::ranges::views::reverse(c_undo.back().undos)) {
		chain_nr_t         nr     = a.nr;
		c_undo_t::modify_t action = a.action;
		board_t            col    = a.bv;

		// get pointers to adjacent chains
		std::vector<chain *> work_chains_gained_liberty;
		for(auto & v: a.chains_gained_liberty) {
			chain *ch = getChain(v, opponentColor(col)).first;
			assert(ch != nullptr);
			work_chains_gained_liberty.emplace_back(ch);
		}

		std::vector<chain *> work_chains_lost_liberty;
		for(auto & v: a.chains_lost_liberty) {
			chain *ch = getChain(v, opponentColor(col)).first;
			assert(ch != nullptr);
			work_chains_lost_liberty.emplace_back(ch);
		}

		// chain was added? then remove it now
		// should be 1 element
		if (action == c_undo_t::modify_t::A_ADD) {
			auto c = getChain(nr, col);

			// remove chain
			for(auto & stone: *c.first->getStones()) {
				const int place = stone.getV();

				// remove stone from chainmap
				assert(cm[place] != NO_CHAIN);
				cm[place] = NO_CHAIN;

				// remove liberty from adjacent chains
				for(auto & ch: work_chains_gained_liberty)
					ch->removeLiberty(stone);

				// and add back to who lost them
				for(auto & ch: work_chains_lost_liberty)
					ch->addLiberty(stone);
			}

			removeChain(col, nr);

			delete c.first;
		}
		else if (action == c_undo_t::modify_t::A_REMOVE) {
			chain *c = new chain();

			// re-register the re-created old chain
			addChain(col, nr, c);

			// recreate chain
			c->addStones(a.stones);
			c->addLiberties(a.liberties);

			// add the stones to the chainmap
			for(auto & stone: a.stones) {
				mapChain(stone, nr);

				// remove liberty from adjacent chains
				for(auto & ch: work_chains_gained_liberty)
					ch->removeLiberty(stone);

				// and add back to who lost them
				for(auto & ch: work_chains_lost_liberty)
					ch->addLiberty(stone);
			}
		}
		else if (action == c_undo_t::modify_t::A_MODIFY) {
			auto c = getChain(nr, col);

			c.first->removeStones(a.stones);
			printf("*** HALLO ***\n");
			// TODO etc
		}
		else {
			assert(0);
		}
	}

	c_undo.pop_back();
	printf("*** UNDO OUTPUT:\n");
	dump();
	printf("* UNDO with hash %016lx finished * --------------------------------\n", hash);

	assert(b_undo.size() == c_undo.size());
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

	dumpUndoSet(true);

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

			if (chain.second == NO_CHAIN)
				line += "    -";
			else
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

        line += "   ";

        for(int x=0; x<dim; x++)
                line += "     ";

        line += "   ";

        for(int x=0; x<dim; x++) {
                int xc = 'A' + x;

                if (xc >= 'I')
                        xc++;

                line += myformat(" %c", xc);
        }

        printf("%s\n", line.c_str());
}

void Board::dumpUndoSet(const bool full)
{
	if (full) {
		size_t n = b_undo.size();

		for(size_t i=0; i<n; i++) {
			b_undo.at(i).dump();
			printf("\n");
			c_undo.at(i).dump();
			printf("\n");
			printf("\n");
		}
	}
	else {
		b_undo.back().dump();
		printf("\n");
		c_undo.back().dump();
		printf("\n");
		printf("\n");
	}
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
