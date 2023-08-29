#include <cassert>
#include <cstdio>
#include <cstring>

#include "board.h"
#include "str.h"
#include "time.h"


void unit_tests()
{
	Zobrist z(9);
#if 0
	// test == function for empty boards
	{
		Board a(&z, 9), b(&z, 9);
		assert(a == b);
	}

	// test != function for empty boards of different size
	{
		Board a(&z, 9), b(&z, 19);
		assert(a != b);
	}

	// test != function for board with a stone on it (without finish)
	{
		Board a(&z, 9), b(&z, 9);
		b.startMove();
		b.putAt(Vertex(0, 0, 9), board_t::B_BLACK);
		assert(a != b);
	}

	// test != function for board with a stone on it (with finish)
	{
		Board a(&z, 9), b(&z, 9);
		b.startMove();
		b.putAt(Vertex(0, 0, 9), board_t::B_BLACK);
		b.finishMove();
		assert(a != b);
	}

	// test == function for board with a stone on it and then undo
	{
		Board a(&z, 9), b(&z, 9);
		b.startMove();
		b.putAt(Vertex(0, 0, 9), board_t::B_BLACK);
		b.finishMove();

		assert(b.getUndoDepth() == 1);

		b.undoMoveSet();

		assert(a == b);
	}

	// test == function for board with a stone on it and then undo to empty
	{
		Board a(&z, 9), b(&z, 9);
		b.startMove();
		b.putAt(Vertex(0, 0, 9), board_t::B_BLACK);
		b.putAt(Vertex(3, 2, 9), board_t::B_WHITE);
		b.putAt(Vertex(1, 1, 9), board_t::B_BLACK);
		b.putAt(Vertex(7, 8, 9), board_t::B_WHITE);
		b.finishMove();

		b.undoMoveSet();

		assert(a == b);
	}

	// test == function for board with a stone on it and then undo to a non-empty
	{
		Board a(&z, 9), b(&z, 9);

		a.startMove();
		a.putAt(Vertex(1, 3, 9), board_t::B_BLACK);
		a.putAt(Vertex(4, 4, 9), board_t::B_WHITE);
		a.finishMove();

		b.startMove();
		b.putAt(Vertex(1, 3, 9), board_t::B_BLACK);
		b.putAt(Vertex(4, 4, 9), board_t::B_WHITE);
		b.finishMove();

		b.startMove();
		b.putAt(Vertex(0, 0, 9), board_t::B_BLACK);
		b.putAt(Vertex(3, 2, 9), board_t::B_WHITE);
		b.putAt(Vertex(1, 1, 9), board_t::B_BLACK);
		b.putAt(Vertex(7, 8, 9), board_t::B_WHITE);
		b.finishMove();

		assert(a.getUndoDepth() == 1);
		assert(b.getUndoDepth() == 2);

		b.undoMoveSet();

		assert(b.getUndoDepth() == 1);

		assert(a == b);
	}

	// test board assignment
	{
		Board a(&z, 19), b(&z, 9);

		b.startMove();
		b.putAt(Vertex(0, 0, 9), board_t::B_BLACK);
		b.putAt(Vertex(3, 2, 9), board_t::B_WHITE);
		b.putAt(Vertex(1, 1, 9), board_t::B_BLACK);
		b.putAt(Vertex(7, 8, 9), board_t::B_WHITE);
		b.finishMove();

		a = b;

		assert(a == b);
	}

	// see if a chain is created when a stone is placed
	{
		Board a(&z, 9);

		a.startMove();
		a.putAt(Vertex(1, 1, 9), board_t::B_BLACK);
		a.finishMove();

		assert(a.getUndoDepth() == 1);

		auto chain = a.getChain(Vertex(1, 1, 9));

		assert(chain.first  != nullptr);
		assert(chain.second != 0);

		assert(a.getBlackChains()->size() == 1);
		assert(a.getWhiteChains()->empty());

		assert(a.getBlackChains()->begin()->second->getLiberties()->size() == 4);
	}

	// see if a chain is removed when undo is invoked
	{
		Board a(&z, 9);

		a.startMove();
		a.putAt(Vertex(1, 1, 9), board_t::B_BLACK);
		a.finishMove();

		assert(a.getUndoDepth() == 1);

		a.undoMoveSet();

		assert(a.getUndoDepth() == 0);

		auto chain = a.getChain(Vertex(1, 1, 9));

		assert(chain.first  == nullptr);
		assert(chain.second == 0);

		assert(a.getAt(Vertex(1, 1, 9)) == board_t::B_EMPTY);

		assert(a.getBlackChains()->empty());
		assert(a.getWhiteChains()->empty());
	}

	// see if a chain is removed that others are not affected
	{
		Board a(&z, 9);

		a.startMove();
		a.putAt(Vertex(1, 1, 9), board_t::B_BLACK);
		a.finishMove();

		a.startMove();
		a.putAt(Vertex(7, 7, 9), board_t::B_BLACK);
		a.finishMove();

		a.undoMoveSet();

		auto chain = a.getChain(Vertex(1, 1, 9));

		assert(chain.first  != nullptr);
		assert(chain.second != 0);

		chain = a.getChain(Vertex(7, 7, 9));

		assert(chain.first  == nullptr);
		assert(chain.second == 0);

		assert(a.getAt(Vertex(7, 7, 9)) == board_t::B_EMPTY);

		assert(a.getBlackChains()->size() == 1);
		assert(a.getWhiteChains()->empty());

		assert(a.getBlackChains()->begin()->second->getLiberties()->size() == 4);
	}

	// see if chains are merged
	{
		Board a(&z, 9);

		a.startMove();
		a.putAt(Vertex(1, 1, 9), board_t::B_BLACK);
		a.finishMove();

		a.startMove();
		a.putAt(Vertex(3, 1, 9), board_t::B_BLACK);
		a.finishMove();

		a.startMove();
		a.putAt(Vertex(2, 1, 9), board_t::B_BLACK);
		a.finishMove();

		assert(a.getUndoDepth() == 3);

		auto chain1 = a.getChain(Vertex(1, 1, 9));
		auto chain2 = a.getChain(Vertex(2, 1, 9));
		auto chain3 = a.getChain(Vertex(3, 1, 9));

		assert(chain1.first  == chain2.first  && chain1.first  == chain3.first);
		assert(chain1.second == chain2.second && chain1.second == chain3.second);

		assert(chain1.first  != nullptr);
		assert(chain1.second != 0);

		assert(a.getBlackChains()->size() == 1);
		assert(a.getWhiteChains()->empty());

		assert(a.getBlackChains()->begin()->second->getLiberties()->size() == 8);
	}

	// see if a chain is removed when a chain is dead
	{
		Board a(&z, 9);

		a.startMove();
		a.putAt(Vertex(4, 4, 9), board_t::B_BLACK);
		a.finishMove();

		a.startMove();
		a.putAt(Vertex(4, 3, 9), board_t::B_WHITE);  // above
		a.putAt(Vertex(4, 5, 9), board_t::B_WHITE);  // below
		a.putAt(Vertex(3, 4, 9), board_t::B_WHITE);  // left
		a.putAt(Vertex(5, 4, 9), board_t::B_WHITE);  // right
		a.finishMove();

		assert(a.getUndoDepth() == 2);

		auto chain = a.getChain(Vertex(4, 4, 9));

		assert(chain.first  == nullptr);
		assert(chain.second == 0);

		assert(a.getBlackChains()->empty());
		assert(a.getWhiteChains()->size() == 4);

		for(auto & c: *a.getBlackChains())
			assert(c.second->getLiberties()->size() == 4);
	}

	// verify restored chain and liberty counts after undo
	{
		Board a(&z, 9);

		Vertex testV(4, 4, 9);
		a.startMove();
		a.putAt(testV, board_t::B_BLACK);
		a.finishMove();

		auto prev_data = a.getChain(testV);
		assert(prev_data.first  != nullptr);
		assert(prev_data.second != 0);

		a.startMove();
		a.putAt(Vertex(4, 3, 9), board_t::B_WHITE);  // above
		a.putAt(Vertex(4, 5, 9), board_t::B_WHITE);  // below
		a.putAt(Vertex(3, 4, 9), board_t::B_WHITE);  // left
		a.finishMove();

		a.startMove();
		a.putAt(Vertex(5, 4, 9), board_t::B_WHITE);  // right
		a.finishMove();

		assert(a.getUndoDepth() == 3);

		a.undoMoveSet();

		assert(a.getUndoDepth() == 2);

		assert(a.getAt(testV) == board_t::B_BLACK);  // board check
		assert(a.getChain(testV).second == prev_data.second);  // chain map check (index)
		assert(a.getChain(testV).first->getLiberties()->size() == 1);

		assert(a.getChain(Vertex(4, 3, 9)).first->getLiberties()->size() == 3);
		assert(a.getChain(Vertex(4, 5, 9)).first->getLiberties()->size() == 3);
		assert(a.getChain(Vertex(3, 4, 9)).first->getLiberties()->size() == 3);
	}
#endif
	// verify restored chain and liberty counts after multiple undo levels
	{
		Board a(&z, 9);

		printf("1. create black\n");

		Vertex testV(4, 4, 9);
		a.startMove();
		a.putAt(testV, board_t::B_BLACK);
		a.finishMove();

		auto prev_data = a.getChain(testV);
		assert(prev_data.first  != nullptr);
		assert(prev_data.second != 0);

		printf("2. white above/below\n");

		a.startMove();
		a.putAt(Vertex(4, 3, 9), board_t::B_WHITE);  // above
		a.finishMove();
		a.startMove();
		a.putAt(Vertex(4, 5, 9), board_t::B_WHITE);  // below
		a.finishMove();

		printf("3. random left\n");

		a.startMove();
		a.putAt(Vertex(4, 2, 9), board_t::B_WHITE);
		a.finishMove();

		printf("4. white left/right\n");

		a.startMove();
		a.putAt(Vertex(3, 4, 9), board_t::B_WHITE);  // left
		a.finishMove();
		a.startMove();
		a.putAt(Vertex(5, 4, 9), board_t::B_WHITE);  // right
		a.finishMove();
		printf("5. center is gone\n");
		a.undoMoveSet();
		printf("6A. center is back\n");
		printf("6B. left/right is back\n");

		a.undoMoveSet();

		assert(a.getUndoDepth() == 4);

		a.undoMoveSet();

		assert(a.getUndoDepth() == 3);

		assert(a.getAt(testV) == board_t::B_BLACK);  // board check
		assert(a.getChain(testV).second == prev_data.second);  // chain map check (index)
		assert(a.getChain(testV).first->getLiberties()->size() == 1);

		assert(a.getChain(Vertex(4, 3, 9)).first->getLiberties()->size() == 3);
		assert(a.getChain(Vertex(4, 5, 9)).first->getLiberties()->size() == 3);
		assert(a.getChain(Vertex(3, 4, 9)).first->getLiberties()->size() == 3);
	}

	printf("All good\n");
}

uint64_t perft_do(Board & b, std::unordered_set<uint64_t> *const seen, const board_t bv, const int depth, const int pass, const bool verbose, const bool top)
{
	printf(" ======> DEPTH %d <=====\n", depth);
	printf("%s\n", b.dumpFEN(bv, 0).c_str());

	if (depth == 0)
		return 1;

	if (pass >= 2)
		return 0;

	const int     new_depth  = depth - 1;
	const board_t new_player = bv == board_t::B_BLACK ? board_t::B_WHITE : board_t::B_BLACK;

	uint64_t      total      = 0;

	std::vector<Vertex> *liberties = b.findLiberties(bv);

	for(auto & cross : *liberties) {
		b.startMove();
		b.putAt(cross, bv);
		b.finishMove();

		uint64_t hash = b.getHash();

		if (seen->find(hash) == seen->end()) {
			seen->insert(hash);

			uint64_t cur_count = perft_do(b, seen, new_player, new_depth, 0, verbose, false);

			total += cur_count;

			if (verbose && top)
				printf("%c%d: %ld\n", cross.getX() + 'a', cross.getY() + 1, cur_count);

			seen->erase(hash);
		}

		b.undoMoveSet();
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

void perft(const int dim, const int depth)
{
	Zobrist z(dim);

	Board b(&z, dim);

	std::unordered_set<uint64_t> seen;

	uint64_t start_ts = get_ts_ms();

	uint64_t total = perft_do(b, &seen, board_t::B_BLACK, depth, 0, 1, 1);

	uint64_t end_ts = get_ts_ms();

	uint64_t took = end_ts - start_ts;

	printf("Took: %.3fs, %f nps\n", took / 1000., total * 1000. / took);
}

int main(int argc, char *argv[])
{
	if (argc == 1 || strcmp(argv[1], "unit-tests") == 0)
		unit_tests();
	else if (argc == 4 || strcmp(argv[1], "perft") == 0)
		perft(atoi(argv[2]), atoi(argv[3]));
	else {
		fprintf(stderr, "???\n");
		return 1;
	}

	return 0;
}
