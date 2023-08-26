#include <cassert>
#include <cstring>
#include <ranges>

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

void Board::updateField(const Vertex & v, const board_t bv)
{
	int place = v.getV();

	assert(undo.back().finished == false);
	undo.back().undos.emplace_back(v, b[place], hash);

	if (b[place] != B_EMPTY)
		hash ^= z->get(place, b[place] == B_BLACK);

	b[place] = bv;

	if (bv != B_EMPTY)
		hash ^= z->get(place, bv == B_BLACK);
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
				b[o] = B_WHITE, hash ^= z->get(o, false);
			else if (c == 'b' || c == 'B')
				b[o] = B_BLACK, hash ^= z->get(o, true);
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
	undo_t u;

	undo.push_back(u);
}

void Board::finishMove()
{
	undo.back().finished = true;
}

void Board::undoMoveSet()
{
	assert(undo.back().finished == true);

	for(auto & tuple: std::ranges::views::reverse(undo.back().undos)) {
		Vertex  & v = std::get<0>(tuple);
		board_t & t = std::get<1>(tuple);

		b[v.getV()] = t;

		hash        = std::get<2>(tuple);
	}

	undo.pop_back();
}

void Board::putAt(const Vertex & v, const board_t bv)
{
	assert(undo.back().finished == false);

	updateField(v, bv);
}

void Board::putAt(const int x, const int y, const board_t bv)
{
	assert(x < dim && x >= 0);
	assert(y < dim && y >= 0);
	assert(undo.back().finished == false);

	updateField({ x, y, dim }, bv);
}
