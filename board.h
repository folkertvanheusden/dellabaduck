#include <stdint.h>
#include <string>

#include "vertex.h"
#include "zobrist.h"


typedef enum { B_EMPTY, B_WHITE, B_BLACK, B_LAST } board_t;

const char *board_t_name(const board_t v);

class Board {
private:
	const Zobrist *const z { nullptr };
	int            dim     { 0       };
	board_t       *b       { nullptr };
	uint64_t       hash    { 0       };

	uint64_t getHashForField(const int v);

public:
	Board(const Zobrist *const z, const int dim);
	Board(const Zobrist *const z, const std::string & str);
	Board(const Zobrist *const z, const Board & bIn);
	~Board();

	int getDim() const;
	void getTo(board_t *const bto) const;
	board_t getAt(const int v) const;
	board_t getAt(const int x, const int y) const;
	uint64_t getHash() const;

	void setAt(const int v, const board_t bv);
	void setAt(const Vertex & v, const board_t bv);
	void setAt(const int x, const int y, const board_t bv);
};
