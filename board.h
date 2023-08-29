#pragma once

#include <cstdint>
#include <map>
#include <string>
#include <unordered_set>
#include <vector>

#include "vertex.h"
#include "zobrist.h"

typedef uint64_t chain_nr_t;

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

	void dump() {
		printf("stones:");

		for(auto & v: squares)
			printf(" %s", v.to_str().c_str());

		printf("\n");

		printf("liberties:");

		for(auto & v: liberties)
			printf(" %s", v.to_str().c_str());

		printf("\n");
	}

	const std::unordered_set<Vertex, Vertex::HashFunction> * getStones() const {
		return &squares;
	}

	const std::unordered_set<Vertex, Vertex::HashFunction> * getLiberties() const {
		return &liberties;
	}

	void addStone(const Vertex & v) {
		bool rc = squares.insert(v).second;
		assert(rc);
		assert(liberties.find(v) == liberties.end());
	}

	void removeStone(const Vertex & v) {
		bool rc = squares.erase(v);
		assert(rc);
	}

	void addLiberty(const Vertex & v) {
		// it is assumed that liberties may be added multiple times
		liberties.insert(v);
	}

	void removeLiberty(const Vertex & v) {
		// it is assumed that liberties can be removed without checking
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

	void dump() {
		printf("finished: %d\n", finished);

		for(auto & undo: undos) {
			Vertex & v = std::get<0>(undo);

			printf("%s: %s\n", v.to_str().c_str(), board_t_name(std::get<1>(undo)));
		}
	}
} b_undo_t;

typedef struct {
	enum class modify_t { A_ADD, A_REMOVE, A_MODIFY };

	const char *modify_t_name(const modify_t m) {
		if (m == modify_t::A_ADD)
			return "add";

		if (m == modify_t::A_REMOVE)
			return "del";

		if (m == modify_t::A_MODIFY)
			return "mod";

		assert(0);

		return "?";
	}

	std::vector<std::tuple<chain_nr_t, modify_t, board_t, std::unordered_set<Vertex, Vertex::HashFunction> > > undos;  // stones
	std::vector<std::tuple<chain_nr_t, Vertex, bool> > undos_liberties;
	bool finished { false };

	void dump() {
		printf("finished: %d\n", finished);

		for(auto & undo: undos) {
			printf("chain %ld, %s, %s\n", std::get<0>(undo), modify_t_name(std::get<1>(undo)), board_t_name(std::get<2>(undo)));
		}

		for(auto & undo: undos_liberties) {
			Vertex & v = std::get<1>(undo);

			printf("liberties %s: %s, chain %ld\n", v.to_str().c_str(), std::get<2>(undo) ? "add" : "del", std::get<0>(undo));
		}
	}
} c_undo_t;

class Board {
private:
	Zobrist *const z    { nullptr };
	int            dim  { 0       };
	board_t       *b    { nullptr };
	uint64_t       hash { 0       };
	chain_nr_t    *cm   { nullptr };  // chain map
	chain_nr_t     cnr  { 1       };  // chain nr (0 = no chain)

	std::map<uint64_t, chain *> blackChains;
	std::map<uint64_t, chain *> whiteChains;

	uint64_t getHashForField(const int v);
	void updateField(const Vertex & v, const board_t bv);

	void addLiberties(chain *const c, const Vertex & v, std::vector<std::tuple<chain_nr_t, Vertex, bool> > *const undo, const uint64_t cnr);

	void getTo(board_t *const bto) const;

	std::vector<b_undo_t> b_undo;

	std::vector<c_undo_t> c_undo;

public:
	Board(Zobrist *const z, const int dim);
	Board(Zobrist *const z, const std::string & str);
	~Board();

	void dump();
	void dumpUndoSet(const bool full);
	std::string dumpFEN(const board_t next_player, const int pass_depth);

	Board & operator=(const Board & in);
	bool operator==(const Board & rhs);
	bool operator!=(const Board & rhs);

	auto getBlackChains() const { return &blackChains; }
	auto getWhiteChains() const { return &whiteChains; }

	std::vector<Vertex> * findLiberties(const board_t bv);

	std::pair<chain *, chain_nr_t> getChain(const Vertex & v);
	std::pair<chain *, chain_nr_t> getChainConst(const Vertex & v) const;
	std::pair<chain *, chain_nr_t> getChain(const chain_nr_t nr, const board_t bv);

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
