#include <assert.h>
#include <limits.h>
#include <random>
#include <stdio.h>
#include <vector>

#include "board.h"
#include "dump.h"
#include "helpers.h"
#include "io.h"
#include "random.h"
#include "score.h"
#include "vertex.h"

bool verifyChainsAndMap(const std::vector<chain_t *> & chainsW, const std::vector<chain_t *> & chainsB, const std::string & name, const ChainMap & cm, const bool verbose)
{
	bool ok = true;

	for(auto chain : chainsW) {
		for(auto & stone : chain->chain) {
			if (cm.getAt(stone) != chain) {
				auto p = cm.getAt(stone);
				send(verbose, "# (%s) stone %s not in map. map: %s, chain: %s", name.c_str(), v2t(stone).c_str(), p ? board_t_name(p->type) : ".", board_t_name(chain->type));
				ok = false;
			}
		}
	}

	for(auto chain : chainsB) {
		for(auto & stone : chain->chain) {
			if (cm.getAt(stone) != chain) {
				auto p = cm.getAt(stone);
				send(verbose, "# (%s) stone %s not in map. map: %s, chain: %s", name.c_str(), v2t(stone).c_str(), p ? board_t_name(p->type) : ".", board_t_name(chain->type));
				ok = false;
			}
		}
	}

	const int dim = cm.getDim();

	for(int y=0; y<dim; y++) {
		for(int x=0; x<dim; x++) {
			Vertex v(x, y, dim);

			auto p = cm.getAt(v);
			if (!p)
				continue;

			auto it = std::find(chainsW.begin(), chainsW.end(), p);
			if (it == chainsW.end())
				it = std::find(chainsB.begin(), chainsB.end(), p);

			if (it == chainsB.end()) {
				send(verbose, "# %s not in either chain", v2t(v).c_str());
				ok = false;
			}
			else {
				auto it2 = std::find((*it)->chain.begin(), (*it)->chain.end(), v);

				if (it2 == (*it)->chain.end()) {
					send(verbose, "# %s not in mapped chain", v2t(v).c_str());
					ok = false;
				}
			}
		}
	}

	return ok;
}

bool test_connect_play(const Board & b, const bool verbose)
{
	bool ok = true;

	Board brd1(b);
	Board brd2(brd1);

	assert(brd1.getHash() == b   .getHash());  // sanity check
	assert(brd2.getHash() == brd1.getHash());  // sanity check

	std::vector<chain_t *> chainsWhite1, chainsBlack1;
	std::vector<chain_t *> chainsWhite2, chainsBlack2;

	ChainMap cm2(brd2.getDim());
	findChains(brd2, &chainsWhite2, &chainsBlack2, &cm2);
	// cm2 now contains state before move

#if 0
	printf("white 2\n");
	checkLibertyPrint(cm2, 2, 0, B_WHITE);
	printf("black 2\n");
	checkLibertyPrint(cm2, 2, 0, B_BLACK);
#endif

	if (!verifyChainsAndMap(chainsWhite2, chainsBlack2, "2A", cm2, verbose))
		ok = false;

	std::vector<Vertex> liberties2W, liberties2B;
	findLiberties(cm2, &liberties2W, B_WHITE);
	findLiberties(cm2, &liberties2B, B_BLACK);

	if (liberties2B.empty() == false) {
		auto move = liberties2B.at(0);

		play(&brd1, move, P_BLACK);

		ChainMap cm1(brd1.getDim());
		findChains(brd1, &chainsWhite1, &chainsBlack1, &cm1);
		// cm1 contains state after move

#if 0
		printf("white 1\n");
		checkLibertyPrint(cm1, 2, 0, B_WHITE);
		printf("black 1\n");
		checkLibertyPrint(cm1, 2, 0, B_BLACK);
#endif

		if (!verifyChainsAndMap(chainsWhite1, chainsBlack1, "1B", cm1, verbose))
			ok = false;

		std::vector<Vertex> liberties1W, liberties1B;
		findLiberties(cm1, &liberties1W, B_WHITE);
		findLiberties(cm1, &liberties1B, B_BLACK);

		connect(&brd2, &cm2, &chainsWhite2, &chainsBlack2, &liberties2W, &liberties2B, playerToStone(P_BLACK), move.getX(), move.getY());
		// cm2 contains state after move

		if (!verifyChainsAndMap(chainsWhite2, chainsBlack2, "2B", cm2, verbose))
			ok = false;

		if (brd2.getHash() != brd1.getHash())
			send(verbose, "boards mismatch"), ok = false;

		if (brd1.getHash() == b   .getHash())
			send(verbose, "boards did not change"), ok = false;

		if (compareChainT(chainsWhite1, chainsWhite2) == false)
			send(verbose, "chainsWhite mismatch"), ok = false;

		if (compareChainT(chainsBlack1, chainsBlack2) == false)
			send(verbose, "chainsBlack mismatch"), ok = false;

		if (compareChain(liberties1W, liberties2W) == false)
			send(verbose, "liberties white mismatch"), ok = false;

		if (compareChain(liberties1B, liberties2B) == false)
			send(verbose, "liberties black mismatch"), ok = false;

		if (!ok) {
			send(true, "# test failed");

			dump(b);

			send(verbose, "# %s", dumpToString(b, P_BLACK, 0).c_str());

			send(verbose, "# move: %s", v2t(move).c_str());

			send(true, " * boards");
			dump(brd1);
			dump(brd2);

			send(true, " * chains black");
			send(true, " * black: play");
			dump(chainsBlack1);
			send(true, " * black: connect");
			dump(chainsBlack2);

			send(true, " * chains white");
			send(true, " * white: play");
			dump(chainsWhite1);
			send(true, " * white: connect");
			dump(chainsWhite2);

			send(true, " * liberties black");
			send(true, "# play(1)");
			dump(liberties1B, true);
			send(true, "# connect(2)");
			dump(liberties2B, true);

			send(true, " * liberties white");
			send(true, "# play(1)");
			dump(liberties1W, true);
			send(true, "# connect(2)");
			dump(liberties2W, true);

			send(true, "---");
		}
	}

	purgeChains(&chainsBlack2);
	purgeChains(&chainsWhite2);

	purgeChains(&chainsBlack1);
	purgeChains(&chainsWhite1);

	return ok;
}

uint64_t perft(const Board & b, std::set<uint64_t> *const seen, const player_t p, const int depth, const int pass, const int verbose, const bool top)
{
	if (depth == 0)
		return 1;

	if (pass >= 2)
		return 0;

	const int      dim        = b.getDim();

	const int      new_depth  = depth - 1;
	const player_t new_player = getOpponent(p);

	uint64_t       total      = 0;

	// find chains of stones
	ChainMap cm(dim);
	std::vector<chain_t *> chainsWhite, chainsBlack;
	findChains(b, &chainsWhite, &chainsBlack, &cm);

	// find the liberties -> the "moves"
	std::vector<Vertex> liberties;
	findLiberties(cm, &liberties, playerToStone(p));

	purgeChains(&chainsBlack);
	purgeChains(&chainsWhite);
	
	for(auto & cross : liberties) {
		Board new_board(b);

		play(&new_board, cross, p);

		uint64_t hash = new_board.getHash();

		if (seen->find(hash) == seen->end()) {
			if (verbose == 2)
				send(true, "%d %s %s %lx", depth, v2t(cross).c_str(), dumpToString(b, p, pass).c_str(), b.getHash());

			seen->insert(hash);

			uint64_t cur_count = perft(new_board, seen, new_player, new_depth, 0, verbose, false);

			total += cur_count;

			if (verbose == 1 && top)
				send(true, "%s: %ld", v2t(cross).c_str(), cur_count);

			seen->erase(hash);
		}
	}

	if (pass < 2) {
		uint64_t cur_count = perft(b, seen, new_player, new_depth, pass + 1, verbose, false);

		total += cur_count;

		if (verbose == 1 && top)
			send(true, "pass: %ld", cur_count);
	}

	if (verbose == 2)
		send(true, "%d pass %s %lx", depth, dumpToString(b, p, pass).c_str(), b.getHash());

	if (verbose == 1 && top)
		send(true, "total: %ld", total);

	return total;
}

void test_perft(const bool verbose, const int dim, const uint64_t *const counts, const size_t n_counts)
{
	send(verbose, "# perft: %zu for %d", n_counts, dim);

	Zobrist z(dim);

	Board b(&z, dim);

	for(size_t i=0; i<n_counts; i++) {
		std::set<uint64_t> seen;

		send(verbose, "# testing depth %zu for %d", i + 1, dim);

		uint64_t count = perft(b, &seen, P_BLACK, i + 1, false, verbose, true);

		if (counts[i] != count)
			send(verbose, "# FAIL depth %zu for %d: expecting %lu, got %lu\n", i + 1, dim, counts[i], count);
	}
}

void test(const bool verbose, const bool with_perft)
{
#if 1
	{
		int dim = 5;
		Zobrist z(dim);
		Board b(&z, "...../...../...../wb.../.w... b 0");

		printf("%d\n", test_connect_play(b, true));

		return;
	}
#endif
	struct test_data {
		std::string b;
		int         dim;
		double      score;
		std::vector<std::pair<std::string, std::string> > white_chains;
		std::vector<std::pair<std::string, std::string> > black_chains;
	};

	std::vector<test_data> boards;

	boards.push_back({
			".....\n"
			".....\n"
			".....\n"
			".....\n"
			".....\n"
			, 5, -7.5,
			{ },
			{ },
			});

	boards.push_back({
			"xxxxxxx\n"
			"x.....x\n"
			"x.o...x\n"
			"x...xxx\n"
			"xxxx...\n"
			".....xx\n"
			".....x.\n"
			, 7, 13.5,
			{ { "C5", "C6 D5 B5 C4" } },
			{ { "G2 F2 F1", "G3 F3 E2 G1 E1" }, { "G7 F7 E7 D7 C7 B7 A7 G6 A6 G5 A5 G4 F4 E4 A4 D3 C3 B3 A3", "F6 E6 D6 C6 B6 F5 E5 B5 D4 C4 B4 G3 F3 E3 D2 C2 B2 A2" } }
			});

	boards.push_back({
			"xxxxxxx\n"
			"x.....x\n"
			"x.....x\n"
			"x...xxx\n"
			"xxxx...\n"
			".....xx\n"
			".o...x.\n"
			, 7, 13.5,
			{ { "B1", "B2 C1 A1" } },
			{ { "G2 F2 F1", "G3 F3 E2 G1 E1" }, { "G7 F7 E7 D7 C7 B7 A7 G6 A6 G5 A5 G4 F4 E4 A4 D3 C3 B3 A3", "F6 E6 D6 C6 B6 F5 E5 B5 D4 C4 B4 G3 F3 E3 D2 C2 B2 A2" } }
		       	});

	boards.push_back({
			"xxxxxxx..\n"
			"x.....x.x\n"
			"x.....x.o\n"
			"x...xxx..\n"
			"xxxx.....\n"
			".....xx..\n"
			".o...x...\n"
			"......ooo\n"
			".....o...\n"
			, 9, 9.5,
			{ { "F1", "F2 G1 E1" }, { "J2 H2 G2", "J3 H3 G3 F2 J1 H1 G1" }, { "B3", "B4 C3 A3 B2" }, { "J7", "H7 J6" } },
			{ { "G4 F4 F3", "G5 F5 H4 E4 G3 E3 F2" }, { "G9 F9 E9 D9 C9 B9 A9 G8 A8 G7 A7 G6 F6 E6 A6 D5 C5 B5 A5", "H9 H8 F8 E8 D8 C8 B8 H7 F7 E7 B7 H6 D6 C6 B6 G5 F5 E5 D4 C4 B4 A4" }, { "J8", "J9 H8" } }
		       	});

	boards.push_back({
			".........\n"
			"...o.....\n"
			"xxox.x...\n"
			"..o...x.x\n"
			".........\n"
			"...o.o.o.\n"
			".........\n"
			".........\n"
			".........\n"
			, 9, -7.5,
			{ { "D4", "D5 E4 C4 D3" }, { "F4", "F5 G4 E4 F3" }, { "H4", "H5 J4 G4 H3" }, { "C7 C6", "C8 D6 B6 C5" }, { "D8", "D9 E8 C8" } },
			{ { "G6", "G7 H6 F6 G5" }, { "J6", "J7 H6 J5" }, { "B7 A7", "B8 A8 B6 A6" }, { "D7", "E7 D6" }, { "F7", "F8 G7 E7 F6" } }
		       	});

#if 0
	boards.push_back({
			"xx.xxxxox\n"
			"xooooxoox\n"
			"xoxo.ooxx\n"
			"oxoxooox.\n"
			"xoxoxoxoo\n"
			"oxoxoxoox\n"
			"xooooooox\n"
			"oooxoxoxx\n"
			"xoxooxoox\n"
			, 9, -15.5,
			{ },
			{ }
			});
#endif

	for(auto b : boards) {
		bool   ok   = true;
		Board  brd  = stringToBoard(b.b);

		double komi       = 7.5;

		auto   temp_score = score(brd, komi);
		double test_score = temp_score.first - temp_score.second;

		if (verbose)
			printf("%s %f\n", dumpToSgf(brd, komi, true).c_str(), test_score);

		if (test_score != b.score)
			send(verbose, "FAIL expected score: %f, current: %f", b.score, test_score), ok = false;

		ChainMap cm(brd.getDim());
		std::vector<chain_t *> chainsWhite, chainsBlack;
		findChains(brd, &chainsWhite, &chainsBlack, &cm);

		scanEnclosed(brd, &cm, playerToStone(P_WHITE));

		if (b.white_chains.size() != chainsWhite.size())
			send(verbose, "FAIL white: number of chains mismatch"), ok = false;

		if (b.black_chains.size() != chainsBlack.size())
			send(verbose, "FAIL black: number of chains mismatch"), ok = false;

		for(auto ch : b.white_chains) {
			auto white_stones = stringToChain(ch.first, brd.getDim());

			if (findChain(chainsWhite, white_stones) == false)
				send(verbose, "FAIL white stones mismatch"), ok = false;
		}

		for(auto ch : b.white_chains) {
			auto white_liberties = stringToChain(ch.second, brd.getDim());

			if (findChain(chainsWhite, white_liberties) == false)
				send(verbose, "FAIL white liberties mismatch for %s", ch.second.c_str()), ok = false;
		}

		for(auto ch : b.black_chains) {
			auto black_stones = stringToChain(ch.first, brd.getDim());

			if (findChain(chainsBlack, black_stones) == false)
				send(verbose, "FAIL black stones mismatch"), ok = false;
		}

		for(auto ch : b.black_chains) {
			auto black_liberties = stringToChain(ch.second, brd.getDim());

			if (findChain(chainsBlack, black_liberties) == false)
				send(verbose, "FAIL black liberties mismatch for %s", ch.second.c_str()), ok = false;
		}

		if (!ok) {
			dump(brd);

			dump(chainsBlack);

			dump(chainsWhite);

			send(true, "---");
		}

		purgeChains(&chainsBlack);
		purgeChains(&chainsWhite);
	}

	// zobrist hashing
	Zobrist z(9);
	Board b(&z, 9);

	uint64_t startHash = b.getHash();
	if (startHash)
		send(verbose, "FAIL initial hash (%lx) invalid", startHash);

	b.setAt(3, 3, B_BLACK);
	uint64_t firstHash = b.getHash();

	if (firstHash == 0)
		send(verbose, "FAIL hash did not change");

	b.setAt(3, 3, B_WHITE);
	uint64_t secondHash = b.getHash();

	if (secondHash == firstHash)
		send(verbose, "FAIL hash (%lx) did not change for type", secondHash);

	if (secondHash == 0)
		send(verbose, "FAIL hash became initial");

	b.setAt(3, 3, B_EMPTY);
	uint64_t thirdHash = b.getHash();

	if (thirdHash)
		send(verbose, "FAIL hash (%lx) did not reset", thirdHash);

	// "connect()"
	for(auto b : boards)
		test_connect_play(stringToBoard(b.b), verbose);

	int ok = 0;
	constexpr int n_to_do = 100000;
	int smallest_n_stones = INT_MAX;

	for(int i=0; i<n_to_do; i++) {
		send(true, "# ===== test %d =====", i);

		int dim = 9;
		Board b(&z, dim);

		// gen
		std::uniform_int_distribution<> rng(0, dim * dim);
		int n = rng(gen);

		std::uniform_int_distribution<> rngdim(0, dim - 1);

		std::uniform_int_distribution<> rngcol(0, 1);

		for(int fill=0; fill<n; fill++) {
			int x = rngdim(gen);
			int y = rngdim(gen);

			if (b.getAt(x, y) == B_EMPTY)
				b.setAt(x, y, rngcol(gen) ? B_BLACK : B_WHITE);
		}

		// purge chains with no liberties
		std::vector<chain_t *> chainsWhite, chainsBlack;

		ChainMap cm(b.getDim());
		findChains(b, &chainsWhite, &chainsBlack, &cm);

		purgeChainsWithoutLiberties(&b, chainsWhite);
		purgeChainsWithoutLiberties(&b, chainsBlack);

		purgeChains(&chainsWhite);
		purgeChains(&chainsBlack);

		// test
		bool cur_ok = test_connect_play(b, verbose);

		ok += cur_ok;

		if (!cur_ok && n < smallest_n_stones) {
			smallest_n_stones = n;

			send(true, "# ---- test %d failed with %d stones ----", i, n);
		}
	}

	send(verbose, "%.1f%% ok (%d), min # stones: %d", ok * 100. / n_to_do, ok, smallest_n_stones);

	if (with_perft) {
		constexpr uint64_t b3x3[] = { 10, 91, 738, 5281, 33384, 179712, 842696, 3271208 };
		constexpr size_t n_b3x3 = sizeof(b3x3) / sizeof(b3x3[0]);

		test_perft(verbose, 3, b3x3, n_b3x3);

		constexpr uint64_t b5x5[] = { 26, 651, 15650, 361041, 7984104 };
		constexpr size_t n_b5x5 = sizeof(b5x5) / sizeof(b5x5[0]);

		test_perft(verbose, 5, b5x5, n_b5x5);
	}

	send(verbose, "--- unittest end ---");
}
