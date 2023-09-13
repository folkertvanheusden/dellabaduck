#include <algorithm>
#include <assert.h>
#include <atomic>
#include <climits>
#include <condition_variable>
#include <cstdint>
#include <ctype.h>
#include <fstream>
#include <limits>
#include <map>
#include <optional>
#include <queue>
#include <random>
#include <set>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <thread>
#include <time.h>
#include <tuple>
#include <unistd.h>
#include <unordered_set>
#include <vector>
#include <sys/resource.h>
#include <sys/time.h>

#include "board.h"
#include "fifo.h"
#include "io.h"
#include "playout.h"
#include "random.h"
#include "score.h"
#include "str.h"
#include "time.h"
#include "uct.h"
#include "vertex.h"
#include "zobrist.h"


Zobrist z(19);

void benchmark(const Board & b, const board_t p, const double komi, const int duration, const std::unordered_set<uint64_t> & seen_in)
{
	uint64_t n     = 0;
	uint64_t nm    = 0;
	uint64_t start = get_ts_ms();

	do {
		auto rc = playout(b, komi, p, seen_in);
		nm += std::get<2>(rc);
		n++;
	}
	while(start + duration > get_ts_ms());

	printf("%f (%.2f)\n", n * 1000. / duration, nm / double(n));
}

std::tuple<Board *, board_t, int> stringToPosition(const std::string & in)
{
	auto parts = split(in, " ");

	Board   *b      = new Board(&z, parts.at(0));

	board_t player = parts.at(1) == "b" || parts.at(1) == "B" ? board_t::B_BLACK : board_t::B_WHITE;

	int      pass   = atoi(parts.at(2).c_str());

	return { b, player, pass };
}

std::optional<Vertex> gen_move(const int move_nr, Board *const b, const board_t & p, const bool do_play, const uint64_t think_end_ts, const double time_left, const double komi, const int n_threads, std::unordered_set<uint64_t> *const seen)
{
	const int dim   = b->getDim();
	const int dimsq = dim * dim;

	std::mutex results_lock;
	std::vector<uint64_t> results;
	results.resize(dimsq);

	uint64_t n     = 0;
	uint64_t nm    = 0;
	uint64_t start = get_ts_ms();

	std::vector<std::thread *> threads;

	for(int i=0; i<n_threads; i++) {
		threads.push_back(new std::thread([&] {
			auto rc = calculate_move(*b, p, think_end_ts, komi, *seen);

			auto & move = std::get<0>(rc);

			if (move.has_value()) {
				std::unique_lock<std::mutex> lck(results_lock);

				n += std::get<1>(rc);

				results.at(move.value().getV()) += std::get<2>(rc);
			}
		}));
	}

	for(auto & t: threads) {
		t->join();

		delete t;
	}

	std::optional<Vertex> v;

	uint64_t best_count = 0;
	int      n_results  = 0;
	for(size_t i=0; i<results.size(); i++) {
		if (results.at(i) == 0)
			continue;

		n_results++;

		if (results.at(i) > best_count) {
			best_count = results.at(i);
			v = Vertex(i, dim);
		}
	}

	if (v.has_value()) {
		uint64_t duration = get_ts_ms() - start;

		send(true, "# move nr %d, best_count %lu for %s in %lu ms (time left: %.2f), %d results, %lu playouts, %.2f moves/playout, %.2f playouts/s",
				move_nr, best_count, v.value().to_str().c_str(), duration, time_left, n_results, n, nm / double(n), n * 1000. / duration);
	}

	if (do_play && v.has_value()) {
		b->startMove();
		b->putAt(v.value(), p);
		b->finishMove();

		send(true, "add hash %lu to seen", b->getHash());

		seen->insert(b->getHash());
	}

	return v;
}

std::string v2t(const Vertex & v)
{
	char xc = 'A' + v.getX();
	if (xc >= 'I')
		xc++;

	return myformat("%c%d", xc, v.getY() + 1);
}

Vertex t2v(const std::string & str, const int dim)
{
	char xc = tolower(str.at(0));
	assert(xc != 'i');
	if (xc >= 'j')
		xc--;
	int x = xc - 'a';

	int y = atoi(str.substr(1).c_str()) - 1;

	return { x, y, dim };
}

int main(int argc, char *argv[])
{
	// uct object is not threadsafe yet
	int nThreads = std::thread::hardware_concurrency();

	int dim = 9;

	std::string logfile;

	int c = -1;
	while((c = getopt(argc, argv, "l:vt:5")) != -1) {
		if (c == 'v')  // console
			setVerbose(true);
		else if (c == 't')
			nThreads = atoi(optarg);
		else if (c == '5')
			dim = 5;
		else if (c == 'l')
			logfile = optarg;
	}

	if (logfile.empty() == false)
		startLog(logfile);

	setbuf(stdout, nullptr);
	setbuf(stderr, nullptr);

	srand(time(nullptr));

	Board   *b    = new Board(&z, dim);

	board_t  p    = board_t::B_BLACK;
	
	int      pass = 0;

	double   komi = 0.;

	double   time_leftB = -1;
	double   time_leftW = -1;

	int      moves_executed = 0;
	int      moves_total    = dim * dim;

	std::unordered_set<uint64_t> seen;

	for(;;) {
		char buffer[4096] { 0 };
		if (!fgets(buffer, sizeof buffer, stdin))
			break;

		char *cr = strchr(buffer, '\r');
		if (cr)
			*cr = 0x00;

		char *lf = strchr(buffer, '\n');
		if (lf)
			*lf = 0x00;

		if (buffer[0] == 0x00)
			continue;

		send(true, "> %s", buffer);

		std::vector<std::string> parts = split(buffer, " ");

		std::string id;
		if (isdigit(parts.at(0).at(0))) {
			id = parts.at(0);
			parts.erase(parts.begin() + 0);
		}

		if (parts.at(0) == "protocol_version")
			send(false, "=%s 2", id.c_str());
		else if (parts.at(0) == "name")
			send(false, "=%s DellaBaduck", id.c_str());
		else if (parts.at(0) == "version")
			send(false, "=%s 0.1", id.c_str());
		else if (parts.at(0) == "boardsize" && parts.size() == 2) {
			delete b;
			b = new Board(&z, atoi(parts.at(1).c_str()));

			send(false, "=%s", id.c_str());
		}
		else if (parts.at(0) == "clear_board") {
			int dim = b->getDim();

			delete b;
			b = new Board(&z, dim);

			seen.clear();

			moves_executed = 0;

			p    = board_t::B_BLACK;
			pass = 0;

			send(false, "=%s", id.c_str());
		}
		else if (parts.at(0) == "play" && parts.size() == 3) {
			p = (parts.at(1) == "b" || parts.at(1) == "black") ? board_t::B_BLACK : board_t::B_WHITE;

			send(false, "=%s", id.c_str());

			if (str_tolower(parts.at(2)) == "pass")
				pass++;
			else {
				Vertex v = t2v(parts.at(2), b->getDim());

				b->startMove();
				b->putAt(v, p);
				b->finishMove();

				seen.insert(b->getHash());

				b->dump();

				pass = 0;
			}

			p = opponentColor(p);

			send(true, "# %s", b->dumpFEN(p, pass).c_str());
		}
		else if (parts.at(0) == "quit") {
			send(false, "=%s", id.c_str());
			break;
		}
		else if (parts.at(0) == "dump") {
			b->dump();
		}
		else if (parts.at(0) == "known_command") {  // TODO
			if (parts.at(1) == "known_command")
				send(false, "=%s true", id.c_str());
			else
				send(false, "=%s false", id.c_str());
		}
		else if (parts.at(0) == "komi") {
			komi = atof(parts.at(1).c_str());

			send(false, "=%s", id.c_str());  // TODO
		}
		else if (parts.at(0) == "time_settings") {
			send(false, "=%s", id.c_str());  // TODO
		}
		else if (parts.at(0) == "time_left" && parts.size() >= 3) {
			board_t player = (parts.at(1) == "b" || parts.at(1) == "black") ? board_t::B_BLACK : board_t::B_WHITE;

			if (player == board_t::B_BLACK)
				time_leftB = atof(parts.at(2).c_str());
			else
				time_leftW = atof(parts.at(2).c_str());

			send(false, "=%s", id.c_str());  // TODO
		}
		else if (parts.at(0) == "list_commands") {
			send(false, "=%s name", id.c_str());
			send(false, "version");
			send(false, "boardsize");
			send(false, "clear_board");
			send(false, "play");
			send(false, "genmove");
			send(false, "komi");
			send(false, "quit");
			send(false, "final_score");
			send(false, "time_settings");
			send(false, "time_left");
		}
		else if (parts.at(0) == "final_score") {
			auto final_score = score(*b, komi);

			send(true, "# black: %f, white: %f", final_score.first, final_score.second);

			send(false, "=%s %s", id.c_str(), scoreStr(final_score).c_str());
		}
		else if (parts.at(0) == "player") {
			send(false, "=%s %c", id.c_str(), p == board_t::B_BLACK ? 'B' : 'W');
		}
		else if (parts.at(0) == "hash") {
			send(false, "=%s %lu", id.c_str(), b->getHash());
		}
		else if (parts.at(0) == "benchmark") {
			int duration = parts.size() == 2 ? atoi(parts.at(1).c_str()) : 1000;

			benchmark(*b, p, komi, duration, seen);
		}
		else if (parts.at(0) == "perft" && (parts.size() == 2 || parts.size() == 3)) {
			int      depth   = atoi(parts.at(1).c_str());

			int      verbose = parts.size() == 3 ? atoi(parts.at(2).c_str()) : 0;

			uint64_t start_t = get_ts_ms();
			uint64_t total   = perft_do(*b, &seen, p, depth, pass, verbose, true);
			uint64_t diff_t  = std::max(uint64_t(1), get_ts_ms() - start_t);

			send(true, "# Total perft for %c and %d passes with depth %d: %lu (%.1f moves per second, %.3f seconds)", p == board_t::B_BLACK ? 'B' : 'W', pass, depth, total, total * 1000. / diff_t, diff_t / 1000.);
		}
		else if (parts.at(0) == "loadstr" && parts.size() == 4) {
			delete b;

			auto new_position = stringToPosition(parts.at(1) + " " + parts.at(2) + " " + parts.at(3));
			b    = std::get<0>(new_position);
			p    = std::get<1>(new_position);
			pass = std::get<2>(new_position);

			seen.insert(b->getHash());
		}
		else if (parts.at(0) == "genmove" || parts.at(0) == "reg_genmove") {
			uint64_t start_ts = get_ts_ms();

			board_t player = (parts.at(1) == "b" || parts.at(1) == "black") ? board_t::B_BLACK : board_t::B_WHITE;

			double time_left = player == board_t::B_BLACK ? time_leftB : time_leftW;

			auto   liberties   = b->findLiberties(player);
			int    n_liberties = liberties.size();

			auto s = score(*b, komi);
			double current_score = player == board_t::B_BLACK ? s.first - s.second : s.second - s.first;
			bool pass_for_win_time = current_score > 0 && time_left <= 0;  // we've won anyway

			bool pass_for_win_pass = pass > 0 && current_score > 0;

			if (n_liberties == 0 || pass_for_win_time || pass_for_win_pass) {
				send(false, "=%s pass", id.c_str());

				pass++;
			}
			else {
				double time_use = time_left / (std::max(n_liberties, moves_total) - moves_executed);

				if (++moves_executed >= moves_total)
					moves_total = (moves_total * 4) / 3;

				send(true, "# use_time: %f", time_use);

				uint64_t think_end_ts = start_ts + time_use * 1000;
				auto     v            = gen_move(moves_executed, b, player, parts.at(0) == "genmove", think_end_ts, time_left, komi, nThreads, &seen);

				time_left = -1.0;

				if (v.has_value()) {
					send(false, "=%s %s", id.c_str(), v2t(v.value()).c_str());

					pass = 0;
				}
				else {
					send(false, "=%s pass", id.c_str());

					pass++;
				}
			}

			b->dump();

			p = opponentColor(player);

			send(true, "# %s", b->dumpFEN(p, pass).c_str());
		}
		else {
			send(false, "?");
		}

		send(false, "");

		for(auto & s: seen)
			send(true, "SEEN %lu", s);

		fflush(nullptr);
	}

	delete b;

	closeLog();

	return 0;
}
