#include <cassert>
#include <cstdio>
#include <cstring>

#include "board.h"
#include "random.h"
#include "str.h"
#include "time.h"


void unit_tests()
{
	Zobrist z(9);

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
		b.finishMove();

		b.startMove();
		b.putAt(Vertex(3, 2, 9), board_t::B_WHITE);
		b.finishMove();

		b.startMove();
		b.putAt(Vertex(1, 1, 9), board_t::B_BLACK);
		b.finishMove();

		b.startMove();
		b.finishMove();
		b.putAt(Vertex(7, 8, 9), board_t::B_WHITE);
		b.finishMove();

		b.undoMoveSet();
		b.undoMoveSet();
		b.undoMoveSet();
		b.undoMoveSet();

		assert(a == b);
	}

	// test == function for board with a stone on it and then undo to a non-empty
	{
		Board a(&z, 9), b(&z, 9);

		a.startMove();
		a.putAt(Vertex(1, 3, 9), board_t::B_BLACK);
		a.finishMove();

		a.startMove();
		a.putAt(Vertex(4, 4, 9), board_t::B_WHITE);
		a.finishMove();

		b.startMove();
		b.putAt(Vertex(1, 3, 9), board_t::B_BLACK);
		b.finishMove();

		b.startMove();
		b.putAt(Vertex(4, 4, 9), board_t::B_WHITE);
		b.finishMove();

		b.startMove();
		b.putAt(Vertex(0, 0, 9), board_t::B_BLACK);
		b.finishMove();

		b.startMove();
		b.putAt(Vertex(3, 2, 9), board_t::B_WHITE);
		b.finishMove();

		b.startMove();
		b.putAt(Vertex(1, 1, 9), board_t::B_BLACK);
		b.finishMove();

		b.startMove();
		b.putAt(Vertex(7, 8, 9), board_t::B_WHITE);
		b.finishMove();

		assert(a.getUndoDepth() == 2);
		assert(b.getUndoDepth() == 6);

		b.undoMoveSet();

		assert(b.getUndoDepth() == 5);

		b.undoMoveSet();
		b.undoMoveSet();
		b.undoMoveSet();

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

	// see if chains are merged (1)
	{
		Board a(&z, 9);

		a.startMove();
		a.putAt(Vertex(1, 1, 9), board_t::B_BLACK);
		a.finishMove();

		a.startMove();
		a.putAt(Vertex(2, 1, 9), board_t::B_BLACK);
		a.finishMove();

		assert(a.getUndoDepth() == 2);

		auto chain1 = a.getChain(Vertex(1, 1, 9));
		auto chain2 = a.getChain(Vertex(2, 1, 9));

		assert(chain1.first  == chain2.first);
		assert(chain1.second == chain2.second);

		assert(chain1.first  != nullptr);
		assert(chain1.second != 0);

		assert(a.getBlackChains()->size() == 1);
		assert(a.getWhiteChains()->empty());

		assert(a.getBlackChains()->begin()->second->getLiberties()->size() == 6);
	}

	// see if chains are merged (2)
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
		a.finishMove();

		a.startMove();
		a.putAt(Vertex(4, 5, 9), board_t::B_WHITE);  // below
		a.finishMove();

		a.startMove();
		a.putAt(Vertex(3, 4, 9), board_t::B_WHITE);  // left
		a.finishMove();

		a.startMove();
		a.putAt(Vertex(5, 4, 9), board_t::B_WHITE);  // right
		a.finishMove();

		assert(a.getUndoDepth() == 5);

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

	// verify restored chain and liberty counts after multiple undo levels
	{
		Board a(&z, 9);

		Vertex testV(4, 4, 9);  // e5
		a.startMove();
		a.putAt(testV, board_t::B_BLACK);
		a.finishMove();

		auto prev_data = a.getChain(testV);
		assert(prev_data.first  != nullptr);
		assert(prev_data.second != 0);

		a.startMove();
		a.putAt(Vertex(4, 3, 9), board_t::B_WHITE);  // above  e4
		a.finishMove();

		a.startMove();
		a.putAt(Vertex(4, 5, 9), board_t::B_WHITE);  // below  e6
		a.finishMove();

		a.startMove();
		a.putAt(Vertex(4, 2, 9), board_t::B_WHITE);  // e3
		a.finishMove();

		a.startMove();
		a.putAt(Vertex(3, 4, 9), board_t::B_WHITE);  // left  d5
		a.finishMove();

		a.startMove();
		a.putAt(Vertex(5, 4, 9), board_t::B_WHITE);  // right  f5
		a.finishMove();

		a.undoMoveSet();  // undo f5

		a.undoMoveSet();  // undo d5

		assert(a.getUndoDepth() == 4);

//		a.dumpUndoSet(false);

//		a.getChain(Vertex(4, 2, 9)).first->dump();

		a.undoMoveSet();  // undo e3

		assert(a.getUndoDepth() == 3);

		assert(a.getAt(testV) == board_t::B_BLACK);  // board check
		assert(a.getChain(testV).second == prev_data.second);  // chain map check (index)

		assert(a.getChain(testV).first->getLiberties()->size() == 2);

		assert(a.getChain(Vertex::from_str("e4", 9)).first->getLiberties()->size() == 3);
		assert(a.getChain(Vertex::from_str("e6", 9)).first->getLiberties()->size() == 3);
	}

	/// situations
	{
		Board a(&z, "...../...../...../..bb./w.b.b b");

		a.startMove();
		a.putAt(Vertex::from_str("d1", 5), board_t::B_BLACK);
		a.finishMove();
	}

	{
		Board a(&z, "........./........./........./........./........./........./........./.bw....../bww...... b");

		a.startMove();
		a.putAt(Vertex::from_str("a2", 9), board_t::B_BLACK);
		a.finishMove();

		a.undoMoveSet();

		a.startMove();
		a.putAt(Vertex::from_str("b3", 9), board_t::B_BLACK);
		a.finishMove();
	}

	{
		Board a(&z, "bww../bww../bbwww/w.w../wbw.. b 0");

		a.startMove();
		a.putAt(Vertex::from_str("d1", 5), board_t::B_BLACK);
		a.finishMove();
	}

#if 0
// .w.bw/wbbbw/w.bww/bbbw./wwww. w 0  b3 e1
// .w.bw/wbbbw/w.bww/bbbw./wwww. w 0  b3 e2
// .w.bw/wbbbw/w.bww/bbbw./wwww. w 0  b3 a5
// .w.bw/wbbbw/w.bww/bbbw./wwww. w 0  b3 c5

	{
		Board a(&z, ".w.bw/wbbbw/w.bww/bbbw./wwww.");

		a.startMove();
		a.putAt(Vertex::from_str("e2", 5), board_t::B_WHITE);
		a.finishMove();

		a.startMove();
		a.putAt(Vertex::from_str("c5", 5), board_t::B_BLACK);
		a.finishMove();

		a.undoMoveSet();

		a.undoMoveSet();

		a.startMove();
		a.putAt(Vertex::from_str("b3", 5), board_t::B_WHITE);
		a.finishMove();

		///

		a.startMove();
		a.putAt(Vertex::from_str("e1", 5), board_t::B_BLACK);
		a.finishMove();

		a.undoMoveSet();

		a.startMove();
		a.putAt(Vertex::from_str("e2", 5), board_t::B_BLACK);
		a.finishMove();

		a.undoMoveSet();

		a.startMove();
		a.putAt(Vertex::from_str("a5", 5), board_t::B_BLACK);
		a.finishMove();

		a.undoMoveSet();

		a.startMove();
		a.putAt(Vertex::from_str("c5", 5), board_t::B_BLACK);
		a.finishMove();

		a.undoMoveSet();

		a.undoMoveSet();
	}

	/*
	{
		Board a(&z, ".w.bw/wbbbw/wbbww/bbbw./wwww. b 0");

		std::vector<Vertex> *liberties = a.findLiberties(board_t::B_BLACK);

		for(auto & cross : *liberties) {
			a.startMove();
			a.putAt(cross, board_t::B_BLACK);
			a.finishMove();

			a.undoMoveSet();
		}
	}
	*/
#endif

	printf("All good\n");
}

uint64_t perft_do(Board & b, std::unordered_set<uint64_t> *const seen, const board_t bv, const int depth, const int pass, const bool verbose, const bool top, std::vector<std::pair<std::string, std::string> > & history)
{
//	printf(" ======> DEPTH %d <=====\n", depth);
//	printf("%s\n", b.dumpFEN(bv, 0).c_str());

	if (depth == 0)
		return 1;

	if (pass >= 2)
		return 0;

	const int     new_depth  = depth - 1;
	const board_t new_player = bv == board_t::B_BLACK ? board_t::B_WHITE : board_t::B_BLACK;

	uint64_t      total      = 0;

	b.dump();

	std::vector<Vertex> *liberties = b.findLiberties(bv);

	for(auto & cross : *liberties) {
/*		if (top) {
		b.dump();

		printf("____ do it: %s\n", cross.to_str().c_str());
		} */

#ifndef NDEBUG
		history.push_back({ cross.to_str(), b.dumpFEN(new_player, 0) });

		printf("%s ", history.begin()->second.c_str());
		for(auto & h: history)
			printf(" %s", h.first.c_str());
		printf("\n");
#endif

//		printf("%s for %s\n", history.back().second.c_str(), history.back().first.c_str());
		b.startMove();
		b.putAt(cross, bv);
		b.finishMove();

/*		if (top) {
			printf("%s %s\n", cross.to_str().c_str(), b.dumpFEN(new_player, 0).c_str());

		printf("____ done it\n");

		b.dump();
		} */

		uint64_t hash = b.getHash();

		if (seen->find(hash) == seen->end()) {
			seen->insert(hash);

			uint64_t cur_count = perft_do(b, seen, new_player, new_depth, 0, verbose, false, history);

			total += cur_count;

			if (verbose && top)
				printf("%c%d: %ld\n", cross.getX() + 'a', cross.getY() + 1, cur_count);

			seen->erase(hash);
		}

	//	printf("UNDO %s for %s\n", cross.to_str().c_str(), b.dumpFEN(bv, 0).c_str());

#ifndef NDEBUG
		history.pop_back();
#endif

		b.undoMoveSet();
	}

	delete liberties;

	if (pass < 2) {
		uint64_t cur_count = perft_do(b, seen, new_player, new_depth, pass + 1, verbose, false, history);

		total += cur_count;

		if (verbose && top)
			printf("pass: %ld\n", cur_count);
	}

	if (verbose && top)
		printf("total: %ld\n", total);

	return total;
}

void perft(const int dim, const int depth, const bool verbose)
{
	Zobrist z(dim);

	Board b(&z, dim);

	std::unordered_set<uint64_t> seen;

	uint64_t start_ts = get_ts_ms();

	std::vector<std::pair<std::string, std::string> > history;

	uint64_t total = perft_do(b, &seen, board_t::B_BLACK, depth, 0, verbose, 1, history);

	uint64_t end_ts = get_ts_ms();

	uint64_t took = end_ts - start_ts;

	printf("Depth %d took: %.3fs, %.2f nps, sum: %lu\n", depth, took / 1000., total * 1000. / took, total);
}

uint64_t perft_fen(const std::string & board_setup, const board_t player, const int depth, const bool verbose)
{
	Zobrist z(1);

	Board b(&z, board_setup);

	std::unordered_set<uint64_t> seen;

	uint64_t start_ts = get_ts_ms();

	std::vector<std::pair<std::string, std::string> > history;

	uint64_t total = perft_do(b, &seen, player, depth, 0, verbose, 1, history);

	uint64_t end_ts = get_ts_ms();

	uint64_t took = end_ts - start_ts;

	printf("Depth %d took: %.3fs, %f nps, sum: %lu\n", depth, took / 1000., total * 1000. / took, total);

	return total;
}

void perfttests()
{
	struct test {
		std::string fen;

		std::vector<uint64_t> counts;
	};

	std::vector<test> tests {
			{ ".w.bw/wbbbw/w.bww/bbbw./wwww. b 0", { 5, 26, 109, 739, 6347, 62970 } },
		};

	for(auto & item: tests) {
		printf("%s\n", item.fen.c_str());

		auto parts = split(item.fen, " ");

		for(size_t depth=1; depth<=item.counts.size(); depth++) {
			uint64_t sum = perft_fen(item.fen, parts.at(1) == "b" ? board_t::B_BLACK : board_t::B_WHITE, depth, false);

			if (sum != item.counts.at(depth - 1))
				printf("Expected sum %lu, got %lu\n", item.counts.at(depth - 1), sum);
		}
	}
}

void randomfill()
{
	const int dim = 5;

	Zobrist z(dim);

	for(;;) {
		Board b(&z, dim);

		board_t player = board_t::B_BLACK;

		for(int i=0; i<dim*dim; i++) {
			std::vector<Vertex> *liberties = b.findLiberties(player);
			if (liberties->empty()) {
				delete liberties;
				break;
			}

			std::uniform_int_distribution<> rng(0, liberties->size() - 1);

			Vertex & v = liberties->at(rng(gen_debug));

			printf("%s %s\n", b.dumpFEN(player, 0).c_str(), v.to_str().c_str());

			b.startMove();
			b.putAt(v, player);
			b.finishMove();

			delete liberties;

			player = opponentColor(player);
		}

		printf("\n---\n");
	}
}

int main(int argc, char *argv[])
{
	if (argc == 1 || strcmp(argv[1], "unit-tests") == 0)
		unit_tests();
	else if (argc >= 4 && strcmp(argv[1], "perft") == 0) {
		int dim = atoi(argv[2]);
		int max = atoi(argv[3]);

		for(int i=1; i<=max; i++)
			perft(dim, i, argc == 5);
	}
	else if (argc == 4 && strcmp(argv[1], "perftone") == 0)
		perft(atoi(argv[2]), atoi(argv[3]), true);
	else if (argc >= 5 && strcmp(argv[1], "perftfen") == 0) {
		int max = atoi(argv[2]);
		std::string fen = argv[3];
		board_t col = toupper(argv[4][0]) == 'B' ? board_t::B_BLACK : board_t::B_WHITE;

		for(int i=1; i<=max; i++)
			perft_fen(fen, col, i, argc >= 6);
	}
	else if (strcmp(argv[1], "perfttests") == 0)
		perfttests();
	else if (strcmp(argv[1], "randomfill") == 0)
		randomfill();
	else {
		fprintf(stderr, "???\n");
		return 1;
	}

	return 0;
}
