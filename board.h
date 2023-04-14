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

class Board {
private:
	Zobrist *const z    { nullptr };
	int            dim  { 0       };
	board_t       *b    { nullptr };
	uint64_t       hash { 0       };

	uint64_t getHashForField(const int v);

public:
	Board(Zobrist *const z, const int dim);
	Board(Zobrist *const z, const std::string & str);
	Board(const Board & bIn);
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


typedef struct {
	board_t type;
	std::vector<Vertex> chain;
	std::set<Vertex> liberties;
} chain_t;

class ChainMap {
private:
	const int       dim      { 0       };
	chain_t **const cm       { nullptr };
	bool *const     enclosed { nullptr };

public:
	ChainMap(const int dim);
	virtual ~ChainMap();

	bool getEnclosed(const int v) const;
	void setEnclosed(const int v);

	int getDim() const;

	chain_t * getAt(const int v) const;
	chain_t * getAt(const Vertex & v) const;
	chain_t * getAt(const int x, const int y) const;

	void setAt(const Vertex & v, chain_t *const chain);
	void setAt(const int x, const int y, chain_t *const chain);
};

void findChainsScan(std::queue<std::pair<unsigned, unsigned> > *const work_queue, const Board & b, unsigned x, unsigned y, const int dx, const int dy, const board_t type, bool *const scanned);
void pickEmptyAround(const ChainMap & cm, const Vertex & v, std::set<Vertex> *const target);
void pickEmptyAround(const Board & b, const Vertex & v, std::set<Vertex> *const target);
void findChains(const Board & b, std::vector<chain_t *> *const chainsWhite, std::vector<chain_t *> *const chainsBlack, ChainMap *const cm);
void findLiberties(const ChainMap & cm, std::vector<Vertex> *const empties, const board_t for_whom);
void scanEnclosed(const Board & b, ChainMap *const cm, const board_t myType);
void purgeChains(std::vector<chain_t *> *const chains);
void connect(Board *const b, ChainMap *const cm, std::vector<chain_t *> *const chainsWhite, std::vector<chain_t *> *const chainsBlack, std::vector<Vertex> *const libertiesWhite, std::vector<Vertex> *const libertiesBlack, const board_t what, const int x, const int y);
void play(Board *const b, const Vertex & v, const player_t & p);
