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

std::pair<chain *, chain_nr_t> Board::getChain(const int o)
{
	assert(o >= 0 && o < dim * dim);

	// get chain number from map
	chain_nr_t nr = cm[o];

	if (nr == NO_CHAIN)
		return { nullptr, nr };

	board_t    bv = b[o];

	auto it       = bv == board_t::B_BLACK ? blackChains.find(nr) : whiteChains.find(nr);

	assert(it != (bv == board_t::B_BLACK ? blackChains.end() : whiteChains.end()));

	return { it->second, nr };
}

std::pair<chain *, chain_nr_t> Board::getChain(const Vertex & v)
{
	assert(v.isValid());

	return getChain(v.getV());
}

std::pair<chain *, chain_nr_t> Board::getChain(const chain_nr_t nr, const board_t bv)
{
	assert(nr != NO_CHAIN);
	assert(bv != board_t::B_EMPTY);

	auto it = bv == board_t::B_BLACK ? blackChains.find(nr) : whiteChains.find(nr);

	assert(it != (bv == board_t::B_BLACK ? blackChains.end() : whiteChains.end()));

	return { it->second, nr };
}

std::pair<chain *, chain_nr_t> Board::getChainConst(const Vertex & v) const
{
	assert(v.isValid());

	const int o = v.getV();
	assert(o < dim * dim);

	chain_nr_t nr = cm[o];

	if (nr == NO_CHAIN) {
		assert(b[o] == board_t::B_EMPTY);

		return { nullptr, nr };
	}

	assert(b[o] != board_t::B_EMPTY);

	auto it = b[o] == board_t::B_BLACK ? blackChains.find(nr) : whiteChains.find(nr);

	assert(it != (b[o] == board_t::B_BLACK ? blackChains.end() : whiteChains.end()));

	return { it->second, nr };
}

void Board::increaseChainNr()
{
	cnr++;

	assert(cnr != NO_CHAIN);
}

void Board::validateBoard()
{
#ifndef NDEBUG
	int dimsq = dim * dim;

	for(int i=0; i<dimsq; i++) {
		if (b[i] != board_t::B_EMPTY) {
			assert(cm[i] != NO_CHAIN);

			if (b[i] == board_t::B_BLACK)
				assert(blackChains.find(cm[i]) != blackChains.end());
			else
				assert(whiteChains.find(cm[i]) != whiteChains.end());
		}
		else {
			assert(cm[i] == NO_CHAIN);
		}
	}

	for(auto & chain: blackChains) {
		for(auto & v: *chain.second->getStones()) {
			assert(b[v.getV()] != board_t::B_EMPTY);

			assert(cm[v.getV()] == chain.first);
		}

		for(auto & v: *chain.second->getLiberties())
			assert(b[v.getV()] == board_t::B_EMPTY);
	}

	for(auto & chain: whiteChains) {
		for(auto & v: *chain.second->getStones()) {
			assert(b[v.getV()] != board_t::B_EMPTY);

			assert(cm[v.getV()] == chain.first);
		}

		for(auto & v: *chain.second->getLiberties())
			assert(b[v.getV()] == board_t::B_EMPTY);
	}
#endif
}

auto Board::getLiberties(chain *const ch, const Vertex & v)
{
	assert(v.isValid());

	std::vector<Vertex> out;

	Vertex vLeft(v.left());
	if (vLeft.isValid() && getAt(vLeft) == board_t::B_EMPTY)
		ch->addLiberty(vLeft);

	Vertex vRight(v.right());
	if (vRight.isValid() && getAt(vRight) == board_t::B_EMPTY)
		ch->addLiberty(vRight);

	Vertex vUp(v.up());
	if (vUp.isValid() && getAt(vUp) == board_t::B_EMPTY)
		ch->addLiberty(vUp);

	Vertex vDown(v.down());
	if (vDown.isValid() && getAt(vDown) == board_t::B_EMPTY)
		ch->addLiberty(vDown);

	return out;
}

auto Board::getSurroundingNonEmptyVertexes(const Vertex & v)
{
	std::vector<Vertex> out;

	Vertex vLeft(v.left());
	if (vLeft.isValid() && getAt(vLeft) != board_t::B_EMPTY)
		out.push_back(vLeft);

	Vertex vRight(v.right());
	if (vRight.isValid() && getAt(vRight) != board_t::B_EMPTY)
		out.push_back(vRight);

	Vertex vUp(v.up());
	if (vUp.isValid() && getAt(vUp) != board_t::B_EMPTY)
		out.push_back(vUp);

	Vertex vDown(v.down());
	if (vDown.isValid() && getAt(vDown) != board_t::B_EMPTY)
		out.push_back(vDown);

	return out;
}

auto Board::getSurroundingChainsOfType(const Vertex & v, const board_t bv)
{
	std::unordered_set<chain_nr_t> out;

	Vertex vLeft(v.left());
	if (vLeft.isValid() && getAt(vLeft) == bv) {
		assert(cm[vLeft.getV()] != NO_CHAIN);
		out.insert(cm[vLeft.getV()]);
	}

	Vertex vRight(v.right());
	if (vRight.isValid() && getAt(vRight) == bv) {
		assert(cm[vRight.getV()] != NO_CHAIN);
		out.insert(cm[vRight.getV()]);
	}

	Vertex vUp(v.up());
	if (vUp.isValid() && getAt(vUp) == bv) {
		assert(cm[vUp.getV()] != NO_CHAIN);
		out.insert(cm[vUp.getV()]);
	}

	Vertex vDown(v.down());
	if (vDown.isValid() && getAt(vDown) == bv) {
		assert(cm[vDown.getV()] != NO_CHAIN);
		out.insert(cm[vDown.getV()]);
	}

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
	assert(cm[v.getV()] != nr);

	cm[v.getV()] = nr;
}

void Board::mapChain(const std::unordered_set<Vertex, Vertex::HashFunction> & chain, const chain_nr_t nr)
{
	for(auto & v: chain)
		mapChain(v, nr);
}

void Board::libertyScan(const std::vector<chain *> & chains)
{
	for(auto & ch: chains) {
		ch->clearLiberties();

		for(auto & v: *ch->getStones())
			getLiberties(ch, v);

		ch->uniqueLiberties();
	}
}

void Board::updateField(const Vertex & v, const board_t bv)
{
//	printf("\n");
//	printf("%s with color %s\n",  v.to_str().c_str(), board_t_name(bv));
	assert(v.isValid());
	assert(v.getDim() == dim);
	assert(bv == board_t::B_BLACK || bv == board_t::B_WHITE);

	validateBoard();

	const int place = v.getV();

	// update layout-undo
	b_undo.back().hash = hash;
	b_undo.back().undos.emplace_back(v, b[place]);
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
	std::vector<Vertex> adjacentBlack;
	std::vector<Vertex> adjacentWhite;

	Vertex vLeft(v.left());
	if (vLeft.isValid()) {
		board_t bv = getAt(vLeft);

		if (bv == board_t::B_BLACK)
			adjacentBlack.push_back(vLeft);
		else if (bv == board_t::B_WHITE)
			adjacentWhite.push_back(vLeft);
	}

	Vertex vRight(v.right());
	if (vRight.isValid()) {
		board_t bv = getAt(vRight);

		if (bv == board_t::B_BLACK)
			adjacentBlack.push_back(vRight);
		else if (bv == board_t::B_WHITE)
			adjacentWhite.push_back(vRight);
	}

	Vertex vUp(v.up());
	if (vUp.isValid()) {
		board_t bv = getAt(vUp);

		if (bv == board_t::B_BLACK)
			adjacentBlack.push_back(vUp);
		else if (bv == board_t::B_WHITE)
			adjacentWhite.push_back(vUp);
	}

	Vertex vDown(v.down());
	if (vDown.isValid()) {
		board_t bv = getAt(vDown);

		if (bv == board_t::B_BLACK)
			adjacentBlack.push_back(vDown);
		else if (bv == board_t::B_WHITE)
			adjacentWhite.push_back(vDown);
	}

	// connect/delete chains when placing a stone
	auto & adjacentMine   = bv == board_t::B_BLACK ? adjacentBlack : adjacentWhite;
	auto & adjacentTheirs = bv == board_t::B_BLACK ? adjacentWhite : adjacentBlack;

	assert(adjacentMine.size() + adjacentTheirs.size() <= 4);

	std::vector<chain *> rescan;

	// connect new stone to existing chain
	if (adjacentMine.size() == 1) {
		// printf("connect new stone to existing chain\n");
		// get chain to connect to
		auto ch = getChain(*adjacentMine.begin());
		assert(ch.first  != nullptr);
		assert(ch.second != NO_CHAIN);
		assert(ch.first->getStones()->empty() == false);

		// connect the new stone to the chain it is adjacent to
		ch.first->addStone(v);

		// update chainmap for the new stone to the chain it is connected to
		mapChain(v, ch.second);

		c_undo_t::action_t action;
		action.nr     = ch.second;  // chain that is modified
		action.action = c_undo_t::modify_t::A_MODIFY;
		action.bv     = bv;  // stone type added
		action.stones = { v };
		action.debug  = 1;
		c_undo.back().undos.push_back(std::move(action));

		rescan.push_back(ch.first);
	}
	// connect adjacent chains
	else if (adjacentMine.empty() == false) {
		// printf("connect adjacent chains\n");
		auto temp = getChain(*adjacentMine.begin());
		chain     *target_c  = temp.first;
		chain_nr_t target_nr = temp.second;

		for(auto & ac: adjacentMine | std::views::drop(1)) {
			assert(ac != *adjacentMine.begin());

			// get pointer of old chain
			auto       ch     = getChain(ac);
			chain     *old_c  = ch.first;
			chain_nr_t old_nr = ch.second;  // get chain number of old chain

			if (old_nr == target_nr)
				continue;

			// add stones of the to-connect-chains to the target chain
			target_c->addStones(*old_c->getStones());

			assert(b[old_c->getStones()->begin()->getV()] == bv);

			// register a chain deletion
			c_undo_t::action_t action_rm;
			action_rm.nr        = old_nr;
			action_rm.action    = c_undo_t::modify_t::A_REMOVE;
			action_rm.bv        = bv;  // stone type removed
			action_rm.stones    = *old_c->getStones();  // these are removed
			action_rm.debug     = 3;
			c_undo.back().undos.push_back(std::move(action_rm));

			// register the modification
			c_undo_t::action_t action_mod;
			action_mod.nr        = target_nr;
			action_mod.action    = c_undo_t::modify_t::A_MODIFY;
			action_mod.bv        = bv;
			action_mod.stones    = *old_c->getStones();
			action_mod.debug     = 2;
			c_undo.back().undos.push_back(std::move(action_mod));

			mapChain(*old_c->getStones(), target_nr);

			// delete chain from lists
			removeChain(bv, old_nr);

			delete old_c;
		}

		// merge the new stone
		target_c->addStone(v);  // add to (new) chain

		mapChain(v, target_nr);

		// register the modification
		c_undo_t::action_t action_add;
		action_add.nr     = target_nr;
		action_add.action = c_undo_t::modify_t::A_MODIFY;
		action_add.bv     = bv;  // stone type added
		action_add.stones = { v };
		action_add.debug  = 4;
		c_undo.back().undos.push_back(std::move(action_add));

		rescan.push_back(target_c);
	}
	else {  // new chain
		// printf("new chain\n");
		chain *new_c = new chain();
		// add the new stone in the new chain
		new_c->addStone(v);

		// put in chainmap
		mapChain(v, cnr);

		// register new chain in the maps
		addChain(bv, cnr, new_c);

		// register that a new chain was registered
		c_undo_t::action_t action;
		action.nr     = cnr;
		action.action = c_undo_t::modify_t::A_ADD;
		action.bv     = bv;  // stone type added
		action.stones = { v };
		action.debug  = 5;
		c_undo.back().undos.push_back(std::move(action));

		increaseChainNr();

		rescan.push_back(new_c);
	}

	for(auto & ac: adjacentTheirs) {
		auto ch = getChain(ac);

		rescan.push_back(ch.first);
	}

	libertyScan(rescan);

	validateBoard();

	// check if any surrounding chains are dead
	for(auto & ac: adjacentTheirs) {
		auto       ch      = getChain(ac);
		chain     *work_c  = ch.first;

		// in case the chain wrapped around the 'v' vertex and thus may have been
		// deleted already
		if (work_c == nullptr)
			continue;

		chain_nr_t work_nr = ch.second;
		board_t    work_b  = opponentColor(bv);  // their color

		if (work_c->isDead() == false)
			continue;

		// clean-up
		for(auto & stone : *work_c->getStones()) {
			assert(b[stone.getV()] == work_b);

			assert(v != stone);

			// update undo record
			b_undo.back().undos.emplace_back(stone, b[stone.getV()]);
			assert(b[stone.getV()] != board_t::B_EMPTY);

			// remove stone from board
			assert(b[stone.getV()] != board_t::B_EMPTY);
			b[stone.getV()] = board_t::B_EMPTY;

			// update hash to no longer include this stone
			hash ^= z->get(stone.getV(), opponentColor(bv) == board_t::B_BLACK);

			// remove stone from chainmap
			mapChain(stone, NO_CHAIN);
		}

		// register a chain deletion
		c_undo_t::action_t action_rm;
		action_rm.nr     = work_nr;
		action_rm.action = c_undo_t::modify_t::A_REMOVE;
		action_rm.bv     = bv == board_t::B_BLACK ? board_t::B_WHITE : board_t::B_BLACK;
		action_rm.stones = *work_c->getStones();  // these are removed
		action_rm.debug  = 6;
		c_undo.back().undos.push_back(std::move(action_rm));

		// remove chain from lists
		removeChain(opponentColor(bv), work_nr);

		delete work_c;
	}

	collectLiberties();

#ifndef NDEBUG
	assert(b[v.getV()] != board_t::B_EMPTY);

	assert(cm[v.getV()] != NO_CHAIN);
#endif

	validateBoard();
}

void Board::collectLiberties()
{
	// TODO: combine with findliberties

	for(auto & ch: whiteChains)
		ch.second->clearLiberties();

	for(auto & ch: blackChains)
		ch.second->clearLiberties();

	const int dimm1 = dim - 1;

	for(int y=0, o=0; y<dim; y++) {
		for(int x=0; x<dim; x++, o++) {
			board_t bv = b[o];

			if (bv != board_t::B_EMPTY)
				continue;

			assert(o == y * dim + x);

			if (x > 0) {
				auto ch = getChain(o - 1);

				if (ch.first)
					ch.first->addLiberty({ o, dim });
			}

			if (x < dimm1) {
				auto ch = getChain(o + 1);

				if (ch.first)
					ch.first->addLiberty({ o, dim });
			}

			if (y > 0) {
				auto ch = getChain(o - dim);

				if (ch.first)
					ch.first->addLiberty({ o, dim });
			}

			if (y < dimm1) {
				auto ch = getChain(o + dim);

				if (ch.first)
					ch.first->addLiberty({ o, dim });
			}
		}
	}
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

			if (c == 'w' || c == 'W') {
				startMove();
				putAt(x, y, board_t::B_WHITE);
				finishMove();
			}
			else if (c == 'b' || c == 'B') {
				startMove();
				putAt(x, y, board_t::B_BLACK);
				finishMove();
			}
			else {
				assert(c == '.');
			}

			str_o++;
		}

		assert(y == 0 || str[str_o] == '/');

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

Board::Board(const Board & in) : z(in.getZobrist())
{
	dim = in.getDim();

	int dimsq = dim * dim;

	b = new board_t[dimsq]();
	cm = new chain_nr_t[dimsq]();

	for(int y=0; y<dim; y++) {
		for(int x=0; x<dim; x++) {
			board_t b = in.getAt(x, y);

			if (b != board_t::B_EMPTY) {
				startMove();
				putAt(x, y, b);
				finishMove();
			}
		}
	}
}

// this ignores undo history!
bool Board::operator==(const Board & rhs)
{
	if (getHash() != rhs.getHash()) {
		printf("hash\n"); return false;
	}

	if (dim != rhs.getDim()) {
		printf("dim\n");
		return false;
	}

	for(int o=0; o<dim * dim; o++) {
		if (getAt(o) != rhs.getAt(o)) {
			printf("field %s\n", Vertex(o, dim).to_str().c_str());
			return false;
		}
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
}

size_t Board::getUndoDepth()
{
	assert(b_undo.size() == c_undo.size());

	return c_undo.size();
}

void Board::undoMoveSet()
{
	// undo stones
	// mostly used for dead chains, not when e.g. replacing chains
	for(auto & tuple: b_undo.back().undos) {
		Vertex  & v = std::get<0>(tuple);
		board_t   t = std::get<1>(tuple);

		b[v.getV()] = t;
	}

	hash = b_undo.back().hash;

	b_undo.pop_back();

	// undo chains
	for(auto & a: std::ranges::views::reverse(c_undo.back().undos)) {
		chain_nr_t         nr     = a.nr;
		c_undo_t::modify_t action = a.action;
		board_t            col    = a.bv;

		// chain was added? then remove it now
		// should be 1 element
		if (action == c_undo_t::modify_t::A_ADD) {
			// printf("revert ADD\n");
			auto c = getChain(nr, col);

			assert(c.first->getStones()->size() == 1);

			// remove chain
			for(auto & stone: *c.first->getStones()) {
				const int place = stone.getV();

				// remove stone from chainmap
				assert(cm[place] != NO_CHAIN);
				cm[place] = NO_CHAIN;
			}

			removeChain(col, nr);

			delete c.first;
		}
		else if (action == c_undo_t::modify_t::A_REMOVE) {
			// printf("revert REMOVE\n");
			chain *c = new chain();

			// re-register the re-created old chain
			addChain(col, nr, c);

			assert(a.stones.size() >= 1);

			// recreate chain
			c->addStones(a.stones);

			// add the stones to the chainmap
			mapChain(a.stones, nr);
		}
		else if (action == c_undo_t::modify_t::A_MODIFY) {
			// printf("revert MODIFY\n");
			auto c = getChain(nr, col);

			assert(a.stones.size() >= 1);

			c.first->removeStones(a.stones);

			mapChain(a.stones, NO_CHAIN);
		}
		else {
			assert(0);
		}
	}

	c_undo.pop_back();

	collectLiberties();

	assert(b_undo.size() == c_undo.size());
}

void Board::putAt(const Vertex & v, const board_t bv)
{
	updateField(v, bv);
}

void Board::putAt(const int x, const int y, const board_t bv)
{
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

void Board::dumpChains()
{
	printf("black chains:\n");
	for(auto & chain: blackChains)
		chain.second->dump();

	printf("white chains:\n");
	for(auto & chain: whiteChains)
		chain.second->dump();

//	dumpUndoSet(true);
}

void Board::dump()
{
        std::string line;

	printf("\n");
	printf("hash: %lu\n", getHash());
        line = "#      board";

	for(int x=0; x<dim-5; x++)
		line += " ";

        line += "   chain";

        for(int x=0; x<dim * 5 - 1; x++)
                line += " ";

	line += "liberties";

        printf("%s\n", line.c_str());

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

uint64_t perft_do(Board & b, std::unordered_set<uint64_t> *const seen, const board_t bv, const int depth, const int pass, const bool verbose, const bool top)
{
//	printf(" ======> DEPTH %d <=====\n", depth);
//	printf("%s\n", b.dumpFEN(bv, 0).c_str());

//	printf("%d\t%s\t%lu\n", depth, b.dumpFEN(bv, 0).c_str(), b.getHash());

	if (depth == 0)
		return 1;

	if (pass >= 2)
		return 0;

	const int     new_depth  = depth - 1;
	const board_t new_player = bv == board_t::B_BLACK ? board_t::B_WHITE : board_t::B_BLACK;

	uint64_t      total      = 0;

	// b.dump();

	std::vector<Vertex> *liberties = b.findLiberties(bv);

	for(auto & cross : *liberties) {
//		printf("____ do it: %s with color %s (hash: %lu)\n", cross.to_str().c_str(), board_t_name(bv), b.getHash());
		//Board copy = b;
//		b.dump();
//		b.dumpChains();

		b.startMove();
		b.putAt(cross, bv);
		b.finishMove();

/*		if (top) {
			printf("%s %s\n", cross.to_str().c_str(), b.dumpFEN(new_player, 0).c_str());

		printf("____ done it\n");

		b.dump();
		} */

		uint64_t hash = b.getHash();

//		printf("GREP %lu\n", hash);

		if (seen->insert(hash).second == true) {
//			printf("%d %s\n", depth, cross.to_str().c_str());

			uint64_t cur_count = perft_do(b, seen, new_player, new_depth, 0, verbose, false);

			total += cur_count;

			if (verbose && top)
				printf("%s: %ld\n", cross.to_str().c_str(), cur_count);

			seen->erase(hash);
		}
//		else
//			printf("%d NOT %s\n", depth, cross.to_str().c_str());

	//	printf("UNDO %s for %s\n", cross.to_str().c_str(), b.dumpFEN(bv, 0).c_str());

		b.undoMoveSet();
//		printf("____ UNDONE it: %s, hash: %lu\n", cross.to_str().c_str(), b.getHash());
//		b.dump();
//		b.dumpChains();
	//	assert(b == copy);
	}

	delete liberties;

	if (pass < 2) {
		uint64_t cur_count = perft_do(b, seen, new_player, new_depth, pass + 1, verbose, false);

		total += cur_count;

		if (verbose && top)
			printf("pass: %ld\n", cur_count);
	}

	if (verbose && top)
		printf("total: %ld\n", total);

	return total;
}
