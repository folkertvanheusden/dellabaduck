#pragma once

#include <queue>
#include <set>
#include <stdint.h>
#include <string>
#include <unordered_set>
#include <vector>

#include "vertex.h"
#include "zobrist.h"


typedef enum { P_BLACK = 0, P_WHITE } player_t;

typedef enum { B_EMPTY, B_WHITE, B_BLACK, B_LAST } board_t;

const char *board_t_name(const board_t v);

typedef struct {
	// vertex, stone, hash
	std::vector<std::tuple<Vertex, board_t, uint64_t> > undos;
	bool finished { false };
} undo_t;

typedef struct {
	std::unordered_set<Vertex, Vertex::HashFunction> squares;
	std::unordered_set<Vertex, Vertex::HashFunction> liberties;
} chain_t;

class Board {
private:
	Zobrist *const z    { nullptr };
	int            dim  { 0       };
	board_t       *b    { nullptr };
	uint64_t       hash { 0       };

	std::vector<chain_t> blackChains;
	std::vector<chain_t> whiteChains;

	uint64_t getHashForField(const int v);
	void updateField(const Vertex & v, const board_t bv);

	void getTo(board_t *const bto) const;

	std::vector<undo_t> undo;

public:
	Board(Zobrist *const z, const int dim);
	Board(Zobrist *const z, const std::string & str);
	Board(const Board & bIn);
	~Board();

	Board & operator=(const Board & in);
	bool operator==(const Board & rhs);
	bool operator!=(const Board & rhs);

	uint64_t getHashForMove(const int v, const board_t bv);

	int getDim() const;
	board_t getAt(const int v) const;
	board_t getAt(const Vertex & v) const;
	board_t getAt(const int x, const int y) const;
	uint64_t getHash() const;

	void startMove();

	void putAt(const int v, const board_t bv);
	void putAt(const Vertex & v, const board_t bv);
	void putAt(const int x, const int y, const board_t bv);

	void finishMove();

	void undoMoveSet();
};
