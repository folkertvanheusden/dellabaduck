#include <assert.h>
#include <string.h>

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

Board::Board(const Zobrist *const z, const int dim) : z(z), dim(dim), b(new board_t[dim * dim]())
{
	assert(dim & 1);
}

Board::Board(const Zobrist *const z, const std::string & str) : z(z)
{
	auto slash = str.find('/');

	dim = slash;
	b = new board_t[dim * dim]();

	int o = 0;

	for(int y=dim - 1; y >= 0; y--) {
		for(int x=0; x<dim; x++) {
			char c = str[o];

			if (c == 'w' || c == 'W')
				setAt(x, y, B_WHITE), hash ^= z->get(o, false);
			else if (c == 'b' || c == 'B')
				setAt(x, y, B_BLACK), hash ^= z->get(o, true);
			else
				assert(c == '.');

			o++;
		}

		o++;  // skip slash
	}
}

Board::Board(const Zobrist *const z, const Board & bIn) : z(z), dim(bIn.getDim()), b(new board_t[dim * dim])
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
