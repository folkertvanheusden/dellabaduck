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
#include "random.h"
#include "score.h"
#include "str.h"
#include "time.h"
#include "vertex.h"
#include "zobrist.h"


Zobrist z(19);

std::tuple<double, double, int, std::optional<Vertex> > playout(const Board & in, const double komi, const board_t p, const std::unordered_set<uint64_t> & seen_in)
{
	Board b(in);

	const int dim   = b.getDim();
	const int dimsq = dim * dim;

	std::unordered_set<uint64_t> seen = seen_in;
	seen.insert(b.getHash());

	int  mc      { 0     };

	bool pass[2] { false };

#ifdef STORE_1_PLAYOUT
	std::string sgf = "(;AP[DellaBaduck]SZ[" + myformat("%d", dim) + "]";

	sgf += myformat(";KM[%f]", komi);
#endif

	board_t for_whom = p;

	std::optional<Vertex> first;

	while(++mc < dim * dim * dim) {
		size_t attempt_n = 0;

	        std::vector<Vertex> *liberties = b.findLiberties(for_whom);

		size_t n_liberties = liberties->size();

		if (n_liberties == 0) {
			delete liberties;
#ifdef STORE_1_PLAYOUT
			sgf += myformat(";%c[pass]", for_whom == board_t::B_BLACK ? 'B' : 'W');
#endif
			break;
		}

                std::uniform_int_distribution<> rng(0, n_liberties - 1);
                int o = rng(gen);

		int d = rng(gen) & 1 ? 1 : -1;

		int x = 0;
		int y =0;

		while(attempt_n < n_liberties) {
			b.startMove();
			b.putAt(liberties->at(o), for_whom);
			b.finishMove();

			// and see if it did not produce a ko
			if (seen.insert(b.getHash()).second == true) {
				x = liberties->at(o).getX();
				y = liberties->at(o).getY();

				if (first.has_value() == false)
					first = liberties->at(o);

				// no ko
				break;  // Ok!
			}

			b.undoMoveSet();

			o += d;

			if (o < 0)
				o = n_liberties - 1;
			else if (o >= n_liberties)
				o = 0;

			attempt_n++;
		}

		delete liberties;

		// all fields tried; pass
		if (attempt_n >= dimsq) {
			pass[for_whom == board_t::B_BLACK] = true;

#ifdef STORE_1_PLAYOUT
			sgf += myformat(";%c[pass]", for_whom == board_t::B_BLACK ? 'B' : 'W');
#endif

			if (pass[0] && pass[1])
				break;

			for_whom = opponentColor(for_whom);

			continue;
		}

		pass[0] = pass[1] = false;

#ifdef STORE_1_PLAYOUT
		sgf += myformat(";%c[%c%c]", for_whom == board_t::B_BLACK ? 'B' : 'W', 'a' + x, 'a' + y);
#endif

		for_whom = opponentColor(for_whom);
	}

	auto s = score(b, komi);

#ifdef STORE_1_PLAYOUT
	sgf += ")";

	printf("%s\n", sgf.c_str());
#endif

	return std::tuple<double, double, int, std::optional<Vertex> >(s.first, s.second, mc, first.value());
}

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

std::optional<Vertex> gen_move(const int move_nr, Board *const b, const board_t & p, const bool do_play, const double use_time, const double time_left, const double komi, const int n_threads, std::unordered_set<uint64_t> *const seen)
{
	if (use_time <= 0.001)
		return { };

	const int dim   = b->getDim();
	const int dimsq = dim * dim;

	send(true, "# use_time: %f", use_time);

	std::mutex results_lock;
	std::vector<std::pair<double, int> > results;
	results.resize(dimsq);

	uint64_t n     = 0;
	uint64_t nm    = 0;
	uint64_t start = get_ts_ms();

	const int duration = use_time * 1000;

	std::vector<std::thread *> threads;

	for(int i=0; i<n_threads; i++) {
		threads.push_back(new std::thread([&] {
			do {
				auto rc = playout(*b, komi, p, *seen);

				if (std::get<3>(rc).has_value()) {  // FIXME (continuing does not make sense here)
					std::unique_lock<std::mutex> lck(results_lock);

					nm += std::get<2>(rc);
					n++;

					auto & move = std::get<3>(rc).value();

					double score = p == board_t::B_BLACK ? std::get<0>(rc) - std::get<1>(rc) : std::get<1>(rc) - std::get<0>(rc);

					double score_v = 0.5;

					if (score < 0)
						score_v = 0.;
					else if (score > 0)
						score_v = 1.;

					results.at(move.getV()).first  += score_v;
					results.at(move.getV()).second++;
				}
			}
			while(start + duration > get_ts_ms());
		}));
	}

	for(auto & t: threads) {
		t->join();

		delete t;
	}

	std::optional<Vertex> v;

	double best = -1.;
	int    best_count = -1;
	int    n_results = 0;
	for(size_t i=0; i<results.size(); i++) {
		if (results.at(i).second == 0)
			continue;

		n_results++;

		double score = results.at(i).first / results.at(i).second;

		if (score > best) {
			best = score;
			best_count = results.at(i).second;
			v = Vertex(i, dim);
		}
	}

	if (v.has_value())
		send(true, "# move nr %d, score %.2f (%d playouts) for %s in %d ms (time left: %.2f), %d results, %lu playouts, %.2f moves/playout, %.2f playouts/s", move_nr, best, best_count, v.value().to_str().c_str(), duration, time_left, n_results, n, nm / double(n), n / use_time);

	if (do_play && v.has_value()) {
		b->startMove();
		b->putAt(v.value(), p);
		b->finishMove();

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
			board_t player = (parts.at(1) == "b" || parts.at(1) == "black") ? board_t::B_BLACK : board_t::B_WHITE;

			double time_left = player == board_t::B_BLACK ? time_leftB : time_leftW;

			if (time_left < 0)
				time_left = 5.0;

			auto   liberties   = b->findLiberties(player);
			int    n_liberties = liberties->size();
			delete liberties;

			if (n_liberties == 0) {
				send(false, "=%s pass", id.c_str());

				pass++;
			}
			else {
				double time_use = time_left / (std::max(n_liberties, moves_total) - moves_executed);

				if (++moves_executed >= moves_total)
					moves_total = (moves_total * 4) / 3;

				uint64_t start_ts = get_ts_ms();
				auto     v        = gen_move(moves_executed, b, player, parts.at(0) == "genmove", time_use, time_left, komi, nThreads, &seen);
				uint64_t end_ts   = get_ts_ms();

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

			p = opponentColor(player);

			send(true, "# %s", b->dumpFEN(p, pass).c_str());
		}
		else {
			send(false, "?");
		}

		send(false, "");

		fflush(nullptr);
	}

	delete b;

	closeLog();

	return 0;
}
