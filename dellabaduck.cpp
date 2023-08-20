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
#include "dump.h"
#include "fifo.h"
#include "helpers.h"
#include "io.h"
#include "random.h"
#include "score.h"
#include "str.h"
#include "time.h"
#include "uct.h"
#include "unittest.h"
#include "vertex.h"
#include "zobrist.h"


//#define CALC_BCO

Zobrist z(19);

typedef struct {
	double score;
	bool valid;
} eval_t;

inline bool isValidMove(const std::vector<chain_t *> & liberties, const Vertex & v)
{
	for(auto chain : liberties) {
		if (chain->liberties.find(v) != chain->liberties.end())
			return true;
	}

	return false;
}

// valid & not enclosed
bool isUsable(const ChainMap & cm, const std::vector<chain_t *> & liberties, const Vertex & v)
{
	return isValidMove(liberties, v) && cm.getEnclosed(v.getV()) == false;
}

void selectRandom(const Board & b, const ChainMap & cm, const std::vector<chain_t *> & chainsWhite, const std::vector<chain_t *> & chainsBlack, std::vector<Vertex> & liberties, const player_t & p, std::vector<eval_t> *const evals)
{
	size_t chainSize = liberties.size();

	size_t r         = 0;

	if (chainSize > 1) {
		std::uniform_int_distribution<> rng(0, chainSize - 2);

		r = rng(gen);
	}

	auto   it        = liberties.begin();

	for(size_t i=0; i<r; i++)
		it++;

	const int v = it->getV();

	evals->at(v).score++;
	evals->at(v).valid = true;
}

void selectExtendChains(const Board & b, const ChainMap & cm, const std::vector<chain_t *> & chainsWhite, const std::vector<chain_t *> & chainsBlack, std::vector<Vertex> & liberties, const player_t & p, std::vector<eval_t> *const evals)
{
	const std::vector<chain_t *> & scan = p == P_BLACK ? chainsWhite : chainsBlack;

	const std::vector<chain_t *> & myLiberties = p == P_BLACK ? chainsBlack : chainsWhite;

	for(auto chain : scan) {
		for(auto chainStone : chain->chain) {
			std::unordered_set<Vertex, Vertex::HashFunction> empties;
			pickEmptyAround(cm, chainStone, &empties);

			for(auto cross : empties) {
				if (isUsable(cm, myLiberties, cross)) {  // TODO: redundant check?
					int v = cross.getV();
					evals->at(v).score += 2;
					evals->at(v).valid = true;
				}
			}
		}
	}
}

void selectKillChains(const Board & b, const ChainMap & cm, const std::vector<chain_t *> & chainsWhite, const std::vector<chain_t *> & chainsBlack, std::vector<Vertex> & liberties, const player_t & p, std::vector<eval_t> *const evals)
{
	const std::vector<chain_t *> & scan = p == P_BLACK ? chainsBlack : chainsWhite;
	const std::vector<chain_t *> & myLiberties = p == P_BLACK ? chainsBlack : chainsWhite;

	for(auto chain : scan) {
		const int add = chain->chain.size() - chain->liberties.size();

		for(auto stone : chain->liberties) {
			if (isUsable(cm, myLiberties, stone)) {
				int v = stone.getV();
				evals->at(v).score += add;
				evals->at(v).valid = true;
			}
		}
	}
}

void selectAtLeastOne(const Board & b, const ChainMap & cm, const std::vector<chain_t *> & chainsWhite, const std::vector<chain_t *> & chainsBlack, std::vector<Vertex> & liberties, const player_t & p, std::vector<eval_t> *const evals)
{
	const std::vector<chain_t *> & myLiberties = p == P_BLACK ? chainsBlack : chainsWhite;

	if (myLiberties.empty())
		return;

	auto      it = myLiberties.at(0)->liberties.begin();
	const int v = it->getV();

	evals->at(v).score++;
	evals->at(v).valid = true;
}

int calcN(const std::vector<chain_t *> & chains)
{
	int n = 0;

	for(auto chain : chains)
		n += chain->chain.size();

	return n;
}

int calcNLiberties(const std::vector<chain_t *> & chains)
{
	int n = 0;

	for(auto chain : chains)
		n += chain->liberties.size();

	return n;
}

typedef struct
{
	std::atomic_bool        flag;
	std::condition_variable cv;
}
end_indicator_t;

#ifdef CALC_BCO
double bco_total = 0;
uint64_t bco_n = 0;
#endif

int search(const Board & b, const player_t & p, int alpha, const int beta, const int depth, const double komi, const uint64_t end_t, end_indicator_t *const ei, std::atomic_bool *const quick_stop)
{
	if (ei->flag || *quick_stop)
		return -32767;

	if (depth == 0) {
		auto s = score(b, komi);
		return p == P_BLACK ? s.first - s.second : s.second - s.first;
	}

	ChainMap cm(b.getDim());
	std::vector<chain_t *> chainsWhite, chainsBlack;
	findChains(b, &chainsWhite, &chainsBlack, &cm);

	std::vector<Vertex> liberties;
	findLiberties(cm, &liberties, playerToStone(p));

	// no valid liberties? return score (eval)
	if (liberties.empty()) {
		purgeChains(&chainsBlack);
		purgeChains(&chainsWhite);

		auto s = score(b, komi);
		return p == P_BLACK ? s.first - s.second : s.second - s.first;
	}

	int bestScore = -32768;
	std::optional<Vertex> bestMove;

	player_t opponent = getOpponent(p);

#ifdef CALC_BCO
	int bco = 0;
#endif

	for(auto stone : liberties) {
		// TODO: check if in liberties van de mogelijke crosses van p
#ifdef CALC_BCO
		bco++;
#endif

		Board work(b);

		play(&work, stone, p);

		int score = -search(work, opponent, -beta, -alpha, depth - 1, komi, end_t, ei, quick_stop);

		if (score > bestScore) {
			bestScore = score;
			bestMove.emplace(stone);

			if (score > alpha) {
				alpha = score;

				if (score >= beta)
					goto finished;
			}
		}
	}

finished:
#ifdef CALC_BCO
	bco_total += double(bco) / nLiberties;
	bco_n++;
#endif

	purgeChains(&chainsBlack);
	purgeChains(&chainsWhite);

	return bestScore;
}

void timer(int think_time, end_indicator_t *const ei)
{
	if (think_time > 0) {
		std::mutex m;
		std::unique_lock<std::mutex> lk(m);

		for(;;) {
			if (ei->cv.wait_for(lk, std::chrono::milliseconds(think_time)) == std::cv_status::timeout) {
				ei->flag = true;
				break;
			}

			if (ei->flag)
				break;
		}
	}
	else {
		ei->flag = true;
	}
}

struct CompareCrossesSortHelper {
	const Board & b;
	const int dim;
	const player_t p;

	CompareCrossesSortHelper(const Board & b, const player_t & p) : b(b), dim(b.getDim()), p(p) {
	}

	int getScore(const int move) {
		Board work(b);

		play(&work, { move, dim }, p);

		auto s = score(work, 0.);

		return p == P_BLACK ? s.first - s.second : s.second - s.first;
	}

	bool operator()(int i, int j) {
		return getScore(i) > getScore(j);
	}
};

void selectAlphaBeta(const Board & b, const ChainMap & cm, const std::vector<chain_t *> & chainsWhite, const std::vector<chain_t *> & chainsBlack, std::vector<Vertex> & liberties, const player_t & p, std::vector<eval_t> *const evals, const double useTime, const double komi, const int nThreads)
{
	const int dim = b.getDim();

	bool *valid = new bool[dim * dim]();

	int n_work = 0;

	std::vector<int> places_for_sort;

	for(auto & v : liberties)
		places_for_sort.emplace_back(v.getV()), n_work++;

	std::sort(places_for_sort.begin(), places_for_sort.end(), CompareCrossesSortHelper(b, p));

	send(true, "# work: %d, time: %f", n_work, useTime);

	uint64_t start_t = get_ts_ms();  // TODO: start of genMove()
	uint64_t hend_t  = start_t + useTime * 1000 / 2;
	uint64_t end_t   = start_t + useTime * 1000;

	end_indicator_t ei { false };
	std::thread *to_timer = new std::thread(timer, end_t - start_t, &ei);

	int depth = 1;
	bool ok = false;

	std::optional<int> global_best;

	int alpha = -32767;
	int beta  =  32767;
	std::mutex a_b_lock;

	while(get_ts_ms() < hend_t && depth <= dim * dim) {
		send(true, "# a/b depth: %d", depth);

		fifo<int> places(dim * dim + 1);

		// queue "work" for threads
		for(auto & v: places_for_sort)
			places.put(v);

		std::atomic_bool quick_stop { false };

		bool allow_next_depth = true;

		ok = false;

		std::vector<std::thread *> threads;

		std::vector<std::optional<std::pair<int, int> > > best;
		best.resize(nThreads);

		for(int i=0; i<nThreads; i++) {
			threads.push_back(new std::thread([hend_t, end_t, &places, dim, b, p, depth, komi, &ei, &alpha, beta, &a_b_lock, &quick_stop, i, &best, &ok, &allow_next_depth] {
						int local_alpha = alpha;
						int local_beta  = beta;

						for(;;) {
							int time_left = hend_t - get_ts_ms();
							if (time_left <= 0 || ei.flag)
								break;

							auto v = places.try_get();

							if (v.has_value() == false) {
								ok = true;
								break;
							}

							Board work(b);

							play(&work, { v.value(), dim }, p);

							int score = search(work, p == P_BLACK ? P_WHITE : P_BLACK, local_alpha, local_beta, depth, komi, end_t, &ei, &quick_stop);

							std::unique_lock<std::mutex> lck(a_b_lock);

							if (ei.flag == false && score > alpha) {
								alpha = score;

								best[i] = { v.value(), score };

								if (score >= beta) {
									send(true, "BCO: %d %d %d\n", alpha, score, beta);
									quick_stop = true;
									ok = true;
									break;
								}

							}

							if (score <= alpha) {
								alpha = -32767;

								allow_next_depth = false;
							}

							local_alpha = alpha;
							local_beta  = beta;
						}
					}));
		}

		send(true, "# %zu threads", threads.size());

		while(threads.empty() == false) {
			(*threads.begin())->join();

			send(true, "# thread terminated, %zu left", threads.size());

			delete *threads.begin();

			threads.erase(threads.begin());
		}

		int                best_score = -32767;
		std::optional<int> best_move;

		for(size_t i=0; i<best.size(); i++) {
			if (best.at(i).has_value() && best.at(i).value().second > best_score) {
				best_move  = best.at(i).value().first;
				best_score = best.at(i).value().second;

				send(true, "# thread %zu chose %s with score %d", i, v2t(Vertex(best_move.value(), dim)).c_str(), best_score);
			}
		}

		if (best_score > alpha)
			alpha = best_score;

		if (ok && ei.flag == false && best_move.has_value()) {
			global_best = best_move;

			send(true, "# Move selected for this depth: %s (%d)", v2t(Vertex(global_best.value(), dim)).c_str(), global_best.value());
		}

		if (allow_next_depth)
			depth++;
		else
			send(true, "# score outside window, retry depth");
	}

	if (to_timer) {
		ei.flag = true;
		ei.cv.notify_one();

		to_timer->join();

		delete to_timer;
	}

	if (global_best.has_value()) {
		send(true, "# Move selected for %c by A/B: %s (reached depth: %d, completed: %d)", p == P_BLACK ? 'B' : 'W', v2t(Vertex(global_best.value(), dim)).c_str(), depth, ok);

		evals->at(global_best.value()).score += 10;
		evals->at(global_best.value()).valid = true;
	}

#ifdef CALC_BCO
	double factor = bco_total / bco_n;
	send(true, "# BCO at %.3f%%; move %d, n: %lu", factor * 100, int(factor * dim * dim), bco_n);
#endif

	delete [] valid;
}

bool isInEye(const Board & b, const int x, const int y, const board_t stone)
{
	const int dim = b.getDim();

	int check_n = 0;
	int fail_n  = 0;

	if (x > 0) {
		check_n++;

		if (b.getAt(x - 1, y) == stone)
			fail_n++;
	}

	if (y > 0) {
		check_n++;

		if (b.getAt(x, y - 1) == stone)
			fail_n++;
	}

	if (x < dim - 1) {
		check_n++;

		if (b.getAt(x + 1, y) == stone)
			fail_n++;
	}

	if (y < dim - 1) {
		check_n++;

		if (b.getAt(x, y + 1) == stone)
			fail_n++;
	}

	return check_n == fail_n;
}

std::tuple<double, double, int> playout(const Board & in, const double komi, player_t p)
{
	Board b(in);

	const int dim = b.getDim();

	// find chains of stones
	ChainMap *cm = new ChainMap(dim);
	std::vector<chain_t *> chainsWhite, chainsBlack;
	findChains(b, &chainsWhite, &chainsBlack, cm);

	std::unordered_set<uint64_t> seen;
	seen.insert(b.getHash());

	int  mc      { 0     };

	bool pass[2] { false };

#ifdef STORE_1_PLAYOUT
	std::string sgf = "(;AP[DellaBaduck]SZ[" + myformat("%d", dim) + "]";

	sgf += myformat(";KM[%f]", komi);
#endif

	while(++mc < dim * dim * dim) {
		std::vector<Vertex> liberties;
		findLiberties(*cm, &liberties, playerToStone(p));

		// no valid liberties? return "pass".
		if (liberties.empty()) {
			pass[p] = true;

			if (pass[0] && pass[1])
				break;

			p = getOpponent(p);

			continue;
		}

		size_t chainSize = liberties.size();

		board_t stone = playerToStone(p);

		std::uniform_int_distribution<> rng(0, chainSize - 1);
		size_t r = 0;
		size_t attempt_n = 0;
		int x = 0, y = 0;

		while(attempt_n < chainSize) {
			// first find a liberty that is not in an eye
			r = rng(gen);

			x = liberties.at(r).getX();
			y = liberties.at(r).getY();

			if (isInEye(b, x, y, stone)) {
				attempt_n++;

				continue;
			}

			Board copyBoard(b);

			// then try the move...
			connect(&b, cm, &chainsWhite, &chainsBlack, stone, x, y);

			// and see if it did not produce a ko
			if (seen.insert(b.getHash()).second == true) {
				// no ko
				pass[0] = pass[1] = false;
				break;
			}

			// ko; rewind

			// 1. rewind board
			b = copyBoard;

			// 2. recreate chains map
			delete cm;
			cm = new ChainMap(dim);
			// purge chains
			purgeChains(&chainsBlack);
			purgeChains(&chainsWhite);
			// find them again
			findChains(b, &chainsWhite, &chainsBlack, cm);

			attempt_n++;
		}

		if (attempt_n == chainSize) {
			pass[p] = true;

			if (pass[0] && pass[1])
				break;

			p = getOpponent(p);

			continue;
		}

		pass[0] = pass[1] = false;

#ifdef STORE_1_PLAYOUT
		Vertex v { x, y, dim };
		sgf += myformat(";%c[%s]", p == P_BLACK ? 'B' : 'W', v2t(v).c_str());
#endif

		p = getOpponent(p);
	}

	purgeChains(&chainsBlack);
	purgeChains(&chainsWhite);

	delete cm;

	auto s = score(b, komi);

#ifdef STORE_1_PLAYOUT
	sgf += ")";

	printf("%s\n", sgf.c_str());
#endif

	return std::tuple<double, double, int>(s.first, s.second, mc);
}

void playoutThread(std::vector<std::pair<double, uint32_t> > *const all_results, std::mutex *const all_results_lock, const uint64_t h_end_t, const uint64_t end_t, const std::vector<Vertex> *const liberties, const player_t p, const double komi, const Board *const b)
{
	auto rc = calculate_move(*b, p, h_end_t - get_ts_ms(), komi);

	int  v  = rc.first.getV();

	std::unique_lock<std::mutex> lck(*all_results_lock);

	all_results->at(v).first  += 8;  // score
	all_results->at(v).second += 1;  // count
}

void selectPlayout(const Board & b, const ChainMap & cm, const std::vector<chain_t *> & chainsWhite, const std::vector<chain_t *> & chainsBlack, const std::vector<Vertex> & liberties, const player_t & p, std::vector<eval_t> *const evals, const double useTime, const double komi, const int nThreads)
{
	uint64_t start_t = get_ts_ms();  // TODO: start of genMove()
	uint64_t h_end_t = start_t + useTime * 450;
	uint64_t end_t   = start_t + useTime * 900;

	std::vector<std::thread *> threads;

	const int dim   = b.getDim();
	const int dimsq = dim * dim;

	std::vector<std::pair<double, uint32_t> > all_results;
	all_results.resize(dimsq);

	std::mutex all_results_lock;

	for(int i=0; i<nThreads; i++)
		threads.push_back(new std::thread(playoutThread, &all_results, &all_results_lock, h_end_t, end_t, &liberties, p, komi, &b));

	while(threads.empty() == false) {
		(*threads.begin())->join();

		delete *threads.begin();

		threads.erase(threads.begin());
	}

	uint64_t total_playouts = 0;

	for(int i=0; i<dimsq; i++) {
		if (all_results.at(i).second) {
			double score = all_results.at(i).first / all_results.at(i).second;

			total_playouts += all_results.at(i).second;

			evals->at(i).score += score;

			evals->at(i).valid = true;
		}
	}

	send(true, "# total playouts: %lu", total_playouts);
}

void purgeKO(const Board & b, const player_t p, std::set<uint64_t> *const seen, std::vector<Vertex> *const liberties)
{
	for(auto it = liberties->begin(); it != liberties->end();) {
		Board temp(b);

		play(&temp, *it, p);

		if (seen->find(temp.getHash()) != seen->end())
			it = liberties->erase(it);
		else
			it++;
	}
}

std::optional<Vertex> genMove(Board *const b, const player_t & p, const bool doPlay, const double useTime, const double komi, const int nThreads, std::set<uint64_t> *const seen)
{
	dump(*b);

	if (useTime <= 0.001)
		return { };

	const int dim = b->getDim();
	const int p2dim = dim * dim;

	// find chains of stones
	ChainMap cm(dim);
	std::vector<chain_t *> chainsWhite, chainsBlack;
	findChains(*b, &chainsWhite, &chainsBlack, &cm);

	std::vector<Vertex> liberties;
	findLiberties(cm, &liberties, playerToStone(p));

	dump(cm);

	dump(chainsBlack);
	dump(chainsWhite);

	dump(liberties);
	purgeKO(*b, p, seen, &liberties);
	dump(liberties);

	// no valid liberties? return "pass".
	if (liberties.empty()) {
		purgeChains(&chainsBlack);
		purgeChains(&chainsWhite);

		return { };
	}

	send(true, "# useTime: %f", useTime);

	std::vector<eval_t> evals;
	evals.resize(p2dim);

	if (useTime >= 0.1)
		// selectAlphaBeta(*b, cm, chainsWhite, chainsBlack, liberties, p, &evals, useTime, komi, nThreads);
		selectPlayout(*b, cm, chainsWhite, chainsBlack, liberties, p, &evals, useTime, komi, nThreads);
	else {
		scanEnclosed(*b, &cm, playerToStone(p));

		selectRandom(*b, cm, chainsWhite, chainsBlack, liberties, p, &evals);

		selectExtendChains(*b, cm, chainsWhite, chainsBlack, liberties, p, &evals);

		selectKillChains(*b, cm, chainsWhite, chainsBlack, liberties, p, &evals);

		selectAtLeastOne(*b, cm, chainsWhite, chainsBlack, liberties, p, &evals);

		selectPlayout(*b, cm, chainsWhite, chainsBlack, liberties, p, &evals, useTime, komi, nThreads);
	}

	// find best
	std::optional<Vertex> v;

	double bestScore = -32767.;
	for(int i=0; i<p2dim; i++) {
		if (evals.at(i).score > bestScore && evals.at(i).valid) {
			Vertex temp { i, dim };

			if (std::find(liberties.begin(), liberties.end(), temp) == liberties.end())
				send(true, "# invalid move %s detected", v2t(temp).c_str());
			else {
				bestScore = evals.at(i).score;
				v.emplace(temp);
			}
		}
	}

	// dump debug
	for(int y=dim-1; y>=0; y--) {
		std::string line = myformat("# %2d | ", y + 1);

		for(int x=0; x<dim; x++) {
			int v = y * dim + x;

			if (evals.at(v).valid)
				line += myformat("%3f ", evals.at(v).score);
			else
				line += myformat("  %s ", board_t_name(b->getAt(x, y)));
		}

		send(true, "%s", line.c_str());
	}

	std::string line = "#      ";
	for(int x=0; x<dim; x++) {
		char c = 'A' + x;
		if (c >= 'I')
			c++;

		line += myformat(" %c  ", c);
	}

	send(true, "%s", line.c_str());

	// remove any chains that no longer have liberties after this move
	// also play the move
	if (doPlay && v.has_value())
		play(b, v.value(), p);

	purgeChains(&chainsBlack);
	purgeChains(&chainsWhite);

	return v;
}

double benchmark_1(const Board & in, const unsigned ms)
{
	send(true, "# starting benchmark 1: duration: %.3fs, board dimensions: %d", ms / 1000.0, in.getDim());

	uint64_t start = get_ts_ms();
	uint64_t end = 0;
	uint64_t n = 0;

	ChainMap cm(in.getDim());

	do {
		std::vector<chain_t *> chainsWhite, chainsBlack;
		findChains(in, &chainsWhite, &chainsBlack, &cm);

		purgeChains(&chainsBlack);
		purgeChains(&chainsWhite);

		n++;

		end = get_ts_ms();
	}
	while(end - start < ms);

	double pops = n * 1000. / (end - start);
	send(true, "# playouts (%lu total) per second: %f", n, pops);

	return pops;
}

double benchmark_2(const Board & in, const unsigned ms)
{
	send(true, "# starting benchmark 2: duration: %.3fs, board dimensions: %d", ms / 1000.0, in.getDim());

	int      dim    = in.getDim();
	int      dimsq  = dim * dim;

	uint64_t start  = get_ts_ms();
	uint64_t end    = 0;
	uint64_t n      = 0;

	uint64_t end_ts = start + ms;

	std::atomic_bool quick_stop { false };

	end_indicator_t  ei { false };

	srand(101);

	do {
		Board work(&z, dim);

		int nstones = (rand() % dimsq) + 1;

		for(int i=0; i<nstones; i++)
			work.setAt(rand() % dimsq, rand() & 1 ? B_WHITE : B_BLACK);

		search(work, P_BLACK, -32767, 32767, 4, 1.5, end_ts, &ei, &quick_stop);

		n++;

		end = get_ts_ms();
	}
	while(end - start < ms);

	double pops = n * 1000. / (end - start);
	send(true, "# playouts (%lu total) per second: %f", n, pops);

	return pops;
}

double benchmark_3(const Board & in, const unsigned ms, const double komi)
{
	send(true, "# starting benchmark 3: duration: %.3fs, board dimensions: %d", ms / 1000.0, in.getDim());

	int      dim    = in.getDim();
	int      dimsq  = dim * dim;

	srand(101);

	Board work(&z, dim);

	int nstones = (rand() % dimsq) + 1;

	for(int i=0; i<nstones; i++)
		work.setAt(rand() % dimsq, rand() & 1 ? B_WHITE : B_BLACK);

	auto rc = calculate_move(work, P_BLACK, ms, komi);

	double pops = rc.second * 1000. / ms;
	send(true, "# playouts (%lu total) per second: %f", rc.second, pops);

	return pops;
}

void load_stones(Board *const b, const char *in, const board_t & bv)
{
	while(in[0] == '[') {
		const char *end = strchr(in, ']');

		char xc = tolower(in[1]);
		char yc = tolower(in[2]);

		int x = xc - 'a';
		int y = yc - 'a';

		b->setAt(x, y, bv);

		in = end + 1;
	}
}

Board loadSgfFile(const std::string & filename)
{
	FILE *sfh = fopen(filename.c_str(), "r");
	if (!sfh) {
		send(false, "# Cannot open %s", filename.c_str());

		return Board(&z, 9);
	}

	char buffer[65536];
	int rc = fread(buffer, 1, sizeof buffer - 1, sfh);

	if (rc == -1) {
		send(false, "# Cannot read from sgf file\n");
		fclose(sfh);
		return Board(&z, 9);
	}

	buffer[rc] = 0x00;

	int dim = 19;

	const char *SZ = strstr(buffer, "SZ[");
	if (SZ)
		dim = atoi(SZ + 3);

	Board b(&z, dim);

	const char *AB = strstr(buffer, "AB[");
	if (AB)
		load_stones(&b, AB + 2, B_BLACK);

	const char *AW = strstr(buffer, "AW[");
	if (AW)
		load_stones(&b, AW + 2, B_WHITE);

	fclose(sfh);

	dump(b);

	return b;
}

Board loadSgf(const std::string & in)
{
	int dim = 9;

	const char *SZ = strstr(in.c_str(), "SZ[");
	if (SZ)
		dim = atoi(SZ + 3);

	Board b(&z, dim);

	size_t o = 0;

	while(o < in.size()) {
		if (in[o] == ';' || in[o] == '(' || in[o] == ')') {
			o++;
			continue;
		}

		auto p = in.find('[', o);
		if (p == std::string::npos) {
			send(false, "# [ missing");
			break;
		}

		std::string name = in.substr(o, p - o);

		std::optional<board_t> player;

		if (name == "B")
			player = B_BLACK;
		else if (name == "W")
			player = B_WHITE;

		auto p_end = in.find(']', p);

		if (p_end == std::string::npos) {
			send(false, "# ] missing");
			break;
		}

		if (player.has_value()) {
			std::string pos = in.substr(p + 1, p_end - (p + 1));

			int x = tolower(pos.at(0)) - 'a';
			int y = atoi(pos.substr(1).c_str()) - 1;

			b.setAt(x, y, player.value());

			// printf("%s: %d,%d/%d\n", pos.c_str(), x, y, player.value());
		}
		else {
			send(false, "# ignoring cross: color \"%s\" not understood", name.c_str());
		}

		o = p_end + 1;
	}

	dump(b);

	return b;
}

int getNEmpty(const Board & b, const player_t p)
{
	ChainMap cm(b.getDim());

	std::vector<chain_t *> chainsWhite, chainsBlack;
	findChains(b, &chainsWhite, &chainsBlack, &cm);

	std::vector<Vertex> liberties;
	findLiberties(cm, &liberties, playerToStone(p));

	purgeChains(&chainsBlack);
	purgeChains(&chainsWhite);

	return liberties.size();
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

	player_t p    = P_BLACK;
	
	int      pass = 0;

	double timeLeftB = -1;
	double timeLeftW = -1;

	double komi = 0.;

	int moves_executed = 0;
	int moves_total    = dim * dim;

	std::string sgf = init_sgf(b->getDim());

	std::set<uint64_t> seen;

#ifdef STORE_1_PLAYOUT
	playout(*b, 7.5, P_BLACK);
	return 0;
#endif

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

			p    = P_BLACK;
			pass = 0;

			send(false, "=%s", id.c_str());

			moves_executed = 0;
			// not /2: assuming that some stones are regained during the game
			moves_total    = dim * dim;

			timeLeftW      = -1;
			timeLeftB      = -1;

			sgf = init_sgf(dim);
		}
		else if (parts.at(0) == "play" && parts.size() == 3) {
			p = (parts.at(1) == "b" || parts.at(1) == "black") ? P_BLACK : P_WHITE;

			send(false, "=%s", id.c_str());

			if (str_tolower(parts.at(2)) == "pass")
				pass++;
			else {
				Vertex v = t2v(parts.at(2), b->getDim());

				play(b, v, p);

				pass = 0;
			}

			sgf += myformat(";%c[%s]", p == P_BLACK ? 'B' : 'W', parts.at(2).c_str());

			send(true, "# %s)", sgf.c_str());

			seen.insert(b->getHash());

			p = getOpponent(p);

			send(true, "# %s", dumpToString(*b, p, 0).c_str());
		}
		else if (parts.at(0) == "debug") {
			dump(*b);

			send(true, "# %s", dumpToString(*b, p, pass).c_str());

			if (parts.size() == 2) {
				ChainMap cm(b->getDim());
				std::vector<chain_t *> chainsWhite, chainsBlack;
				findChains(*b, &chainsWhite, &chainsBlack, &cm);

				auto s = playerToStone((parts.at(1) == "b" || parts.at(1) == "black") ? P_BLACK : P_WHITE);

				send(true, "%c", p == P_BLACK ? 'B' : 'W');

				scanEnclosed(*b, &cm, s);

				dump(chainsBlack);
				dump(chainsWhite);
				dump(cm);

				std::vector<Vertex> liberties;
				findLiberties(cm, &liberties, s);
				dump(liberties);

				purgeChains(&chainsBlack);
				purgeChains(&chainsWhite);
			}
		}
		else if (parts.at(0) == "quit") {
			send(false, "=%s", id.c_str());
			break;
		}
		else if (parts.at(0) == "known_command") {  // TODO
			if (parts.at(1) == "known_command")
				send(false, "=%s true", id.c_str());
			else
				send(false, "=%s false", id.c_str());
		}
		else if (parts.at(0) == "benchmark" && parts.size() == 3) {
			double pops = -1.;

			// play outs per second
			if (parts.at(2) == "1")
				pops = benchmark_1(*b, atoi(parts.at(1).c_str()));
			else if (parts.at(2) == "2")
				pops = benchmark_2(*b, atoi(parts.at(1).c_str()));
			else if (parts.at(2) == "3")
				pops = benchmark_3(*b, atoi(parts.at(1).c_str()), komi);

			send(false, "=%s %f", id.c_str(), pops);
		}
		else if (parts.at(0) == "komi") {
			komi = atof(parts.at(1).c_str());

			send(false, "=%s", id.c_str());  // TODO
							//
			sgf += "KM[" + parts.at(1) + "]";
		}
		else if (parts.at(0) == "time_settings") {
			send(false, "=%s", id.c_str());  // TODO
		}
		else if (parts.at(0) == "time_left" && parts.size() >= 3) {
			player_t player = (parts.at(1) == "b" || parts.at(1) == "black") ? P_BLACK : P_WHITE;

			if (player == P_BLACK)
				timeLeftB = atof(parts.at(2).c_str());
			else
				timeLeftW = atof(parts.at(2).c_str());

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
		else if (parts.at(0) == "final_score") {
			auto final_score = score(*b, komi);

			send(true, "# black: %f, white: %f", final_score.first, final_score.second);

			send(false, "=%s %s", id.c_str(), scoreStr(final_score).c_str());
		}
		else if (parts.at(0) == "unittest") {
			test(std::find(parts.begin() + 1, parts.end(), "-v") != parts.end(), std::find(parts.begin() + 1, parts.end(), "-p") != parts.end());
		}
		else if (parts.at(0) == "loadsgf") {
			delete b;
			b = new Board(loadSgfFile(parts.at(1)));

			seen.clear();
			seen.insert(b->getHash());

			p    = P_BLACK;
			pass = 0;

			send(false, "=%s", id.c_str());
		}
		else if (parts.at(0) == "setsgf") {
			delete b;
			b = new Board(loadSgf(parts.at(1)));

			seen.clear();
			seen.insert(b->getHash());

			p    = P_BLACK;
			pass = 0;

			send(false, "=%s", id.c_str());
		}
		else if (parts.at(0) == "autoplay" && parts.size() == 2) {
			double think_time = atof(parts.at(1).c_str());

			double time_left[] = { think_time, think_time };

			uint64_t g_start_ts = get_ts_ms();
			int n_moves = 0;

			for(;;) {
				n_moves++;

				uint64_t start_ts = get_ts_ms();

				double time_use = time_left[p] / (moves_total - moves_executed);

				if (++moves_executed >= moves_total)
					moves_total = (moves_total * 4) / 3;

				auto v = genMove(b, p, true, time_use, komi, nThreads, &seen);

				uint64_t end_ts = get_ts_ms();

				time_left[p] -= (end_ts - start_ts) / 1000.;

				const char *color = p == P_BLACK ? "black" : "white";

				if (time_left[p] < 0)
					send(true, "# %s is out of time (%f)", color, time_left[p]);

				if (v.has_value() == false)
					break;

				double took = (end_ts - start_ts) / 1000.;

				send(true, "# %s (%s), time allocated: %.3f, took %.3fs (%.2f%%), move-nr: %d, time left: %.3f", v2t(v.value()).c_str(), color, time_use, took, took * 100 / time_use, n_moves, time_left[p]);

				p = getOpponent(p);

				seen.insert(b->getHash());
			}

			uint64_t g_end_ts = get_ts_ms();

			send(true, "# finished %d moves in %.3fs", n_moves, (g_end_ts - g_start_ts) / 1000.0);
		}
		else if (parts.at(0) == "genmove" || parts.at(0) == "reg_genmove") {
			player_t player = (parts.at(1) == "b" || parts.at(1) == "black") ? P_BLACK : P_WHITE;

			double timeLeft = player == P_BLACK ? timeLeftB : timeLeftW;

			if (timeLeft < 0)
				timeLeft = 5.0;

			double time_use = timeLeft / (std::max(getNEmpty(*b, player), moves_total) - moves_executed);

			if (++moves_executed >= moves_total)
				moves_total = (moves_total * 4) / 3;

			uint64_t start_ts = get_ts_ms();
			auto v = genMove(b, player, parts.at(0) == "genmove", time_use, komi, nThreads, &seen);
			uint64_t end_ts = get_ts_ms();

			timeLeft = -1.0;

			if (v.has_value()) {
				send(false, "=%s %s", id.c_str(), v2t(v.value()).c_str());

				sgf += myformat(";%c[%s]", player == P_BLACK ? 'B' : 'W', v2t(v.value()).c_str());

				pass = 0;
			}
			else {
				send(false, "=%s pass", id.c_str());

				pass++;
			}

			send(true, "# %s)", sgf.c_str());

			send(true, "# took %.3fs for %s", (end_ts - start_ts) / 1000.0, v.has_value() ? v2t(v.value()).c_str() : "pass");

			p = getOpponent(player);

			seen.insert(b->getHash());

			send(true, "# %s", dumpToString(*b, p, 0).c_str());
		}
		else if (parts.at(0) == "cputime") {
			struct rusage ru { 0 };

			if (getrusage(RUSAGE_SELF, &ru) == -1)
				send(true, "# getrusage failed");
			else {
				double usage = ru.ru_utime.tv_sec + ru.ru_utime.tv_usec / 1000000.0;

				send(false, "=%s %.4f", id.c_str(), usage);
			}
		}
		else if (parts.at(0) == "player") {
			send(false, "=%s %c", id.c_str(), p == P_BLACK ? 'B' : 'W');
		}
		else if (parts.at(0) == "perft" && (parts.size() == 2 || parts.size() == 3)) {
			int      depth   = atoi(parts.at(1).c_str());

			int      verbose = parts.size() == 3 ? atoi(parts.at(2).c_str()) : 0;

			uint64_t start_t = get_ts_ms();
			uint64_t total   = perft(*b, &seen, p, depth, pass, verbose, true);
			uint64_t diff_t  = std::max(uint64_t(1), get_ts_ms() - start_t);

			send(true, "# Total perft for %c and %d passes with depth %d: %lu (%.1f moves per second, %.3f seconds)", p == P_BLACK ? 'B' : 'W', pass, depth, total, total * 1000. / diff_t, diff_t / 1000.);
		}
		else if (parts.at(0) == "dumpsgf") {
			send(true, "# %s)", sgf.c_str());
		}
		else if (parts.at(0) == "dumpstr") {
			auto out = dumpToString(*b, P_BLACK, 0);

			send(true, "# %s", out.c_str());
		}
		else if (parts.at(0) == "loadstr" && parts.size() == 4) {
			delete b;

			auto new_position = stringToPosition(parts.at(1) + " " + parts.at(2) + " " + parts.at(3));
			b    = std::get<0>(new_position);
			p    = std::get<1>(new_position);
			pass = std::get<2>(new_position);

			seen.insert(b->getHash());

			sgf  = dumpToSgf(*b, komi, false);
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
