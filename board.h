#pragma once

#include <cstdint>
#include <map>
#include <string>
#include <unordered_set>
#include <vector>

#include "vertex.h"
#include "zobrist.h"


enum class player_t { P_BLACK = 0, P_WHITE };

enum class board_t { B_EMPTY, B_WHITE, B_BLACK, B_LAST };

const char *board_t_name(const board_t v);

class chain {
private:
	std::unordered_set<Vertex, Vertex::HashFunction> squares;
	std::unordered_set<Vertex, Vertex::HashFunction> liberties;

public:
	chain() {
	}

	virtual ~chain() {
	}

	const std::unordered_set<Vertex, Vertex::HashFunction> * getStones() const {
		return &squares;
	}

	const std::unordered_set<Vertex, Vertex::HashFunction> * getLiberties() const {
		return &liberties;
	}

	void addStone(const Vertex & v) {
		squares.insert(v);
	}

	void addLiberty(const Vertex & v) {
		liberties.insert(v);
	}

	void removeLiberty(const Vertex & v) {
		liberties.erase(v);
	}

	bool isDead() {
		return liberties.empty();
	}
};

typedef struct {
	// vertex, stone, hash
	std::vector<std::tuple<Vertex, board_t, uint64_t> > undos;
	bool finished { false };
} b_undo_t;

typedef struct {
	// bool: true = add, false = remove
	std::vector<std::tuple<uint64_t, chain *, bool, board_t> > undos;
	std::vector<std::tuple<chain *, Vertex, bool> > undos_liberties;
	bool finished { false };
} c_undo_t;

class Board {
private:
	Zobrist *const z    { nullptr };
	int            dim  { 0       };
	board_t       *b    { nullptr };
	uint64_t       hash { 0       };
	uint64_t      *cm   { nullptr };  // chain map
	uint64_t       cnr  { 1       };  // chain nr (0 = no chain)

	std::map<uint64_t, chain *> blackChains;
	std::map<uint64_t, chain *> whiteChains;

	uint64_t getHashForField(const int v);
	void updateField(const Vertex & v, const board_t bv);

	void addLiberties(chain *const c, const Vertex & v);

	void getTo(board_t *const bto) const;

	std::vector<b_undo_t> b_undo;

	std::vector<c_undo_t> c_undo;

public:
	Board(Zobrist *const z, const int dim);
	Board(Zobrist *const z, const std::string & str);
	Board(const Board & bIn);
	~Board();

	void dump();

	Board & operator=(const Board & in);
	bool operator==(const Board & rhs);
	bool operator!=(const Board & rhs);

	auto getBlackChains() const { return &blackChains; }
	auto getWhiteChains() const { return &whiteChains; }

	std::vector<Vertex> * findLiberties(const board_t for_whom);

	std::pair<chain *, uint64_t> getChain(const Vertex & v);
	std::pair<chain *, uint64_t> getChainConst(const Vertex & v) const;

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

	size_t getUndoDepth();

	void undoMoveSet();
};
