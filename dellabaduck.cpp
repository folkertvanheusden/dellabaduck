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

std::tuple<double, double, int> playout(const Board & in, const double komi, const board_t p)
{
	Board b(in);

	const int dim   = b.getDim();
	const int dimsq = dim * dim;

	std::unordered_set<uint64_t> seen;
	seen.insert(b.getHash());

	int  mc      { 0     };

	bool pass[2] { false };

#ifdef STORE_1_PLAYOUT
	std::string sgf = "(;AP[DellaBaduck]SZ[" + myformat("%d", dim) + "]";

	sgf += myformat(";KM[%f]", komi);
#endif

	board_t for_whom = p;

	while(++mc < dim * dim * dim) {
		size_t attempt_n = 0;

	        std::vector<Vertex> *liberties = b.findLiberties(for_whom);

		size_t n_liberties = liberties->size();

                std::uniform_int_distribution<> rng(0, n_liberties - 1);
                int o = rng(gen);

		int d = rng(gen) & 1 ? 1 : -1;

		while(attempt_n < n_liberties) {
			b.startMove();
			b.putAt(liberties->at(o), for_whom);
			b.finishMove();

			// and see if it did not produce a ko
			if (seen.insert(b.getHash()).second == true) {
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

			if (pass[0] && pass[1])
				break;

			for_whom = opponentColor(for_whom);

			continue;
		}

		pass[0] = pass[1] = false;

#ifdef STORE_1_PLAYOUT
		sgf += myformat(";%c[%c%c]", p == P_BLACK ? 'B' : 'W', 'a' + x, 'a' + y);
#endif

		for_whom = opponentColor(for_whom);
	}

	auto s = score(b, komi);

#ifdef STORE_1_PLAYOUT
	sgf += ")";

	printf("%s\n", sgf.c_str());
#endif

	return std::tuple<double, double, int>(s.first, s.second, mc);
}

void benchmark(const Board & b, const board_t p, const double komi, const int duration)
{
	uint64_t n     = 0;
	uint64_t start = get_ts_ms();

	do {
		auto rc = playout(b, komi, p);
		n++;
	}
	while(start + duration > get_ts_ms());

	printf("%f\n", n * 1000. / duration);
}

std::tuple<Board *, board_t, int> stringToPosition(const std::string & in)
{
	auto parts = split(in, " ");

	Board   *b      = new Board(&z, parts.at(0));

	board_t player = parts.at(1) == "b" || parts.at(1) == "B" ? board_t::B_BLACK : board_t::B_WHITE;

	int      pass   = atoi(parts.at(2).c_str());

	return { b, player, pass };
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
				Vertex v = Vertex::from_str(parts.at(2), b->getDim());

				b->startMove();
				b->putAt(v, p);
				b->finishMove();

				pass = 0;
			}

			seen.insert(b->getHash());

			p = opponentColor(p);
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
		else if (parts.at(0) == "list_commands") {
			send(false, "=%s name", id.c_str());
			send(false, "version");
			send(false, "boardsize");
			send(false, "clear_board");
			send(false, "play");
			send(false, "genmove");
			send(false, "komi");
			send(false, "quit");
			send(false, "loadsgf");
			send(false, "final_score");
			send(false, "time_settings");
			send(false, "time_left");
		}
		else if (parts.at(0) == "player") {
			send(false, "=%s %c", id.c_str(), p == board_t::B_BLACK ? 'B' : 'W');
		}
		else if (parts.at(0) == "hash") {
			send(false, "=%s %lu", id.c_str(), b->getHash());
		}
		else if (parts.at(0) == "benchmark") {
			int duration = parts.size() == 2 ? atoi(parts.at(1).c_str()) : 1000;

			benchmark(*b, p, komi, duration);
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
