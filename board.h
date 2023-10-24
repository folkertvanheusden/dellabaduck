#pragma once

#include <cassert>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "vertex.h"
#include "zobrist.h"

#define NO_CHAIN 0
typedef uint64_t chain_nr_t;

enum class board_t { B_EMPTY, B_WHITE, B_BLACK, B_LAST };

const char *board_t_name(const board_t v);

board_t opponentColor(const board_t v);

class chain {
private:
	std::unordered_set<Vertex, Vertex::HashFunction> stones;
	std::vector<Vertex> liberties;

public:
	chain() {
	}

	chain(const std::unordered_set<Vertex, Vertex::HashFunction> & stones, const std::vector<Vertex> & liberties) :
		stones(stones),
		liberties(liberties) {
	}

	virtual ~chain() {
	}

	chain * duplicate() const {
		return new chain(stones, liberties);
	}

	void dump() {
		printf("stones:");

		for(auto & v: stones)
			printf(" %s", v.to_str().c_str());

		printf("\n");

		printf("liberties:");

		for(auto & v: liberties)
			printf(" %s", v.to_str().c_str());

		printf("\n");
	}

	const std::unordered_set<Vertex, Vertex::HashFunction> * getStones() const {
		return &stones;
	}

	const std::vector<Vertex> * getLiberties() const {
		return &liberties;
	}

	void addStone(const Vertex & v) {
		bool rc = stones.insert(v).second;
		assert(rc);
	}

	void addStones(const std::unordered_set<Vertex, Vertex::HashFunction> & in) {
		stones.insert(in.begin(), in.end());
	}

	void removeStone(const Vertex & v) {
		bool rc = stones.erase(v);
		assert(rc);
	}

	void removeStones(const std::unordered_set<Vertex, Vertex::HashFunction> & in) {
		for(auto & v: in) {
			bool rc = stones.erase(v);
			assert(rc);
		}
	}

	void clearLiberties() {
		liberties.clear();
	}

	void uniqueLiberties() {
		std::sort(liberties.begin(), liberties.end());
		liberties.erase(std::unique(liberties.begin(), liberties.end()), liberties.end());
	}

	void addLiberty(const Vertex & v) {
		liberties.push_back(v);
	}

	bool isDead() {
		return liberties.empty();
	}
};

typedef struct {
	// vertex, stone, hash
	std::vector<std::tuple<Vertex, board_t> > undos;
	uint64_t hash;

	void dump() {
		printf("board undo actions:");

		for(auto & undo: undos) {
			Vertex & v = std::get<0>(undo);

			printf(" %s|%s", v.to_str().c_str(), board_t_name(std::get<1>(undo)));
		}

		printf("\n");
	}
} b_undo_t;

typedef struct {
	enum class modify_t { A_ADD, A_REMOVE, A_MODIFY };

	typedef struct {
		chain_nr_t nr;
		modify_t   action;
		board_t    bv;
		std::unordered_set<Vertex, Vertex::HashFunction> stones;
		int        debug;
	} action_t;

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

	std::vector<action_t> undos;

	void dump() {
		printf("chain undo actions:\n");

		for(auto & undo: undos) {
			printf("  chain: %lu, action: %s, color: %s, debug: %d\n", undo.nr, modify_t_name(undo.action), board_t_name(undo.bv), undo.debug);

			printf("    stones:");
			for(auto & v: undo.stones)
				printf(" %s", v.to_str().c_str());
			printf("\n");
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

	std::unordered_map<uint64_t, chain *> chainGroups[2];

	int board_tToChainGroupNr(const board_t bv) const;

	// helper for copy/assignment
	void assign(const Board & in);

	void increaseChainNr();

	void updateField(const Vertex & v, const board_t bv);

	void addChain(const board_t bv, chain_nr_t cnr, chain *const new_c);
	void mapChain(const Vertex & v, const chain_nr_t nr);
	void mapChain(const std::unordered_set<Vertex, Vertex::HashFunction> & chain, const chain_nr_t nr);
	void removeChain(const board_t bv, const chain_nr_t nr);

	void getLiberties(chain *const ch, const Vertex & v);
	void libertyScan(const std::vector<chain *> & chains);
	void libertyScan(const std::unordered_set<int> & places);
	void collectLiberties();

	void getTo(board_t *const bto) const;

	std::vector<b_undo_t> b_undo;

	std::vector<c_undo_t> c_undo;

	Zobrist *getZobrist() const { return z; }

	chain_nr_t getCMAt(const int o) const { return cm[o]; }

	inline auto getChainGroup(const int nr) const { return &chainGroups[nr]; }

public:
	Board(Zobrist *const z, const int dim);
	Board(Zobrist *const z, const std::string & str);
	Board(const Board & in);
	~Board();

	void validateBoard();

	void dump();
	void dumpUndoSet(const bool full);
	void dumpChains();
	std::string dumpFEN(const board_t next_player, const int pass_depth) const;

	Board & operator=(const Board & in);
	bool operator==(const Board & rhs) const;
	bool operator==(const Board & rhs);
	bool operator!=(const Board & rhs);

	auto getBlackChains() const { return &chainGroups[board_tToChainGroupNr(board_t::B_BLACK)]; }
	auto getWhiteChains() const { return &chainGroups[board_tToChainGroupNr(board_t::B_WHITE)]; }

	std::vector<Vertex> findLiberties(const board_t bv);

	std::pair<chain *, chain_nr_t> getChain(const int o);
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

	// used by unittest
	void setAt(const int x, const int y, const board_t bv) { assert(x >= 0 && x < dim && y >= 0 && y < dim); b[y * dim + x] = bv; }
};

uint64_t perft_do(Board & b, std::unordered_set<uint64_t> *const seen, const board_t bv, const int depth, const int pass, const bool verbose, const bool top);
