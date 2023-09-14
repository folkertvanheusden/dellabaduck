#include <cassert>
#include <cstdio>
#include <cstring>

#include "board.h"
#include "io.h"
#include "random.h"
#include "str.h"
#include "time.h"
#include "uct.h"


void unit_tests()
{
	srand(1234);

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

		assert(a != b);

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

	// test hash after copy (and modification)
	{
		Board a(&z, "...../...../...../..bb./w.b.b b");

		Board b = a;
		assert(b.getHash() == a.getHash());

		Vertex move = Vertex::from_str("d3", 5);
		a.startMove();
		a.putAt(move, board_t::B_BLACK);
		a.finishMove();
		b.startMove();
		b.putAt(move, board_t::B_BLACK);
		b.finishMove();

		assert(b.getHash() == a.getHash());
	}

	// compare boards 1
	for(int i=0; i<128; i++) {
		for(int dim=3; dim<21; dim += 2) {
			Board a(&z, dim);
			Board b = a;

			for(int fill=0; fill<dim * dim * 3 / 4; fill++)
				a.setAt(rand() % dim, rand() % dim, rand() & 1 ? board_t::B_BLACK : board_t::B_WHITE);

			assert(b != a);

			b = a;

			assert(b == a);
		}
	}

	// compare boards with history
	srand(1234);
	std::unordered_set<uint64_t> seen;
	for(int dim=3; dim<21; dim += 2) {
		for(int i=0; i<128; i++) {
			Board a(&z, dim);

			for(int fill=0; fill<dim * dim * 3 / 4; fill++) {
				a.setAt(rand() % dim, rand() % dim, rand() & 1 ? board_t::B_BLACK : board_t::B_WHITE);

				seen.insert(a.getHash());
			}
		}
	}
	for(int dim=3; dim<21; dim += 2) {
		for(int i=0; i<128; i++) {
			Board a(&z, dim);

			for(int fill=0; fill<dim * dim * 3 / 4; fill++) {
				a.setAt(rand() % dim, rand() % dim, rand() & 1 ? board_t::B_BLACK : board_t::B_WHITE);

				assert(seen.find(a.getHash()) != seen.end());
			}
		}
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

	{
		Board a(&z, "b.bw..w.b/bb.bwwbb./wwbwwwbwb/.bb.wwwww/wb.bwwww./b.b...wbw/......b.b/b.......b/bb.....bb w 0");
		Board b(&z, "b.bw..w.b/bb.bwwbb./wwbwwwbwb/.bb.wwwww/wb.bwwww./b.b...wbw/......b.b/b.......b/bb.....bb w 0");

		for(int i=0; i<3; i++) {
			a.startMove();
			a.putAt(Vertex::from_str("i8", 9), board_t::B_WHITE);
			a.finishMove();

			assert(a.getHash() != b.getHash());

			a.startMove();
			a.putAt(Vertex::from_str("i7", 9), board_t::B_BLACK);
			a.finishMove();

			assert(a.getHash() == b.getHash());
		}
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

	{
		Board a(&z, "...../...../...../..bb./w.b.b b");
		const uint64_t compare_hash = a.getHash();

		auto liberties = a.findLiberties(board_t::B_BLACK);
		size_t n = liberties.size();
		assert(n >= 2);
		size_t n_seen = n / 2;

		// half of the liberties should not be allowed
		std::unordered_set<uint64_t> seen;

		for(size_t i=0; i<n_seen; i++) {
			a.startMove();
			a.putAt(liberties.at(i), board_t::B_BLACK);
			a.finishMove();

			uint64_t hash = a.getHash();
			send(true, " * adding %lu (%s) to the ignore list", hash, liberties.at(i).to_str().c_str());
			assert(seen.insert(hash).second);

			a.undoMoveSet();
			assert(a.getHash() == compare_hash);
			assert(seen.size() == i + 1);
		}

		// uct_node creation test
		for(size_t i=0; i<n; i++) {
			uct_node u(nullptr, a, board_t::B_BLACK, liberties.at(i), 7.5, seen, 0);
			assert(u.is_valid() == (i >= n_seen));
			assert(a.getHash() == compare_hash);
			assert(seen.size() == n_seen);
		}

		// playout test
		for(size_t i=0; i<n; i++) {
			auto rc = calculate_move(a, board_t::B_BLACK, get_ts_ms() + 1000, 7.5, 3, seen);
			assert(a.getHash() == compare_hash);
			assert(seen.size() == n_seen);

			auto move = std::get<0>(rc);

			assert(move.has_value());

			size_t idx = std::find(liberties.begin(), liberties.end(), move.value()) - liberties.begin();
			send(true, "found %zu, limit %zu, %s", idx, n_seen, move.value().to_str().c_str());
			assert(idx >= n_seen);
		}
	}

	{
		for(int i=0; i<9; i++) {
#ifdef SITUATIONAL_SUPERKO
			assert(z.get(i, true) != z.get(i, false));
#else
			assert(z.get(i, true) == z.get(i, false));
#endif
		}
	}

	printf("All good\n");
}

void perft(const int dim, const int depth, const bool verbose)
{
	Zobrist z(dim);

	Board b(&z, dim);

	std::unordered_set<uint64_t> seen;
	seen.insert(b.getHash());

	uint64_t start_ts = get_ts_ms();

	uint64_t total = perft_do(b, &seen, board_t::B_BLACK, depth, 0, verbose, 1);

	uint64_t end_ts = get_ts_ms();

	uint64_t took = end_ts - start_ts;

	printf("Depth %d took: %.3fs, %.2f nps, sum: %lu\n", depth, took / 1000., total * 1000. / took, total);
}

uint64_t perft_fen(const std::string & board_setup, const board_t player, const int depth, const bool verbose)
{
	Zobrist z(1);

	Board b(&z, board_setup);

	std::unordered_set<uint64_t> seen;
	seen.insert(b.getHash());

	uint64_t start_ts = get_ts_ms();

	uint64_t total = perft_do(b, &seen, player, depth, 0, verbose, 1);

	uint64_t end_ts = get_ts_ms();

	uint64_t took = end_ts - start_ts;

	printf("Depth %d took: %.3fs, %.2f nps, sum: %lu\n", depth, took / 1000., total * 1000. / took, total);

	return total;
}

void perfttests()
{
	struct test {
		std::string fen;

		std::vector<uint64_t> counts;
	};

	std::vector<test> tests {
			{ ".../.../... b 0", { 10, 91, 738, 5281, 33384, 179712, 842696 } },
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
	// TODO
}

void benchmark()
{
	uint64_t start_ts = get_ts_ms();
	uint64_t end_ts = 0;
	uint64_t n = 0;

	Zobrist z(9);
	Board b(&z, 9);

	srand(123);
	for(int i=0; i<60; i++)
		b.setAt(rand() % 9, rand() % 9, rand() & 1 ? board_t::B_WHITE : board_t::B_BLACK);

	do {
		Board copy = b;

		n++;
		end_ts = get_ts_ms();
	}
	while(end_ts - start_ts <= 2500);

	printf("%.2f\n", n * 1000.0 / (end_ts - start_ts));
}

int main(int argc, char *argv[])
{
	startLog("board_unittest.log");

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
	else if (strcmp(argv[1], "benchmark") == 0)
		benchmark();
	else {
		fprintf(stderr, "???\n");
		return 1;
	}

	return 0;
}
