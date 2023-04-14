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
#include "vertex.h"
#include "zobrist.h"


//#define CALC_BCO

Zobrist z(19);

std::string init_sgf(const int dim)
{
	return "(;AP[DellaBaduck]SZ[" + myformat("%d", dim) + "]";
}

std::string dumpToSgf(const Board & b, const double komi, const bool with_end)
{
	int         dim = b.getDim();
	std::string sgf = init_sgf(dim);

	sgf += myformat(";KM[%f]", komi);

	for(int y=0; y<dim; y++) {
		for(int x=0; x<dim; x++) {
			auto v     = Vertex(x, y, dim);

			auto stone = b.getAt(v.getV());

			if (stone == B_EMPTY)
				continue;

			sgf += myformat(";%c[%s]", stone == B_BLACK ? 'B' : 'W', v2t(v).c_str());
		}
	}

	return sgf + (with_end ? ")" : "");
}

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

	const int v = liberties.at(r).getV();

	evals->at(v).score++;
	evals->at(v).valid = true;
}

void selectExtendChains(const Board & b, const ChainMap & cm, const std::vector<chain_t *> & chainsWhite, const std::vector<chain_t *> & chainsBlack, std::vector<Vertex> & liberties, const player_t & p, std::vector<eval_t> *const evals)
{
	const std::vector<chain_t *> & scan = p == P_BLACK ? chainsWhite : chainsBlack;

	const std::vector<chain_t *> & myLiberties = p == P_BLACK ? chainsBlack : chainsWhite;

	for(auto chain : scan) {
		for(auto chainStone : chain->chain) {
			std::set<Vertex> empties;
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
		places_for_sort.push_back(v.getV()), n_work++;

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

std::tuple<double, double, int> playout(const Board & in, const double komi, player_t p)
{
	Board b(in);

	// find chains of stones
	ChainMap cm(b.getDim());
	std::vector<chain_t *> chainsWhite, chainsBlack;
	findChains(b, &chainsWhite, &chainsBlack, &cm);

	std::vector<Vertex> liberties;
	findLiberties(cm, &liberties, playerToStone(p));

	int  mc      { 0     };

	bool pass[2] { false };

	while(++mc < 250) {
		// no valid liberties? return "pass".
		if (liberties.empty()) {
			pass[p] = true;

			if (pass[0] && pass[1])
				break;

			p = getOpponent(p);

			continue;
		}

		pass[0] = pass[1] = false;

		size_t r  = 0;

		size_t chainSize = liberties.size();

		if (chainSize > 1) {
			std::uniform_int_distribution<> rng(0, chainSize - 2);

			r = rng(gen);
		}

		const int x = liberties.at(r).getX();
		const int y = liberties.at(r).getY();

		// TODO: pass liberties[] (for each color) to connect
		connect(&b, &cm, &chainsWhite, &chainsBlack, &liberties, playerToStone(p), x, y);

		p = getOpponent(p);
	}

	purgeChains(&chainsBlack);
	purgeChains(&chainsWhite);

	auto s = score(b, komi);

	return std::tuple<double, double, int>(s.first, s.second, mc);
}

void playoutThread(std::vector<std::pair<double, uint32_t> > *const all_results, std::mutex *const all_results_lock, const uint64_t h_end_t, const uint64_t end_t, const std::vector<Vertex> *const liberties, const player_t p, const double komi, const Board *const b)
{
	const int dim   = b->getDim();
	const int dimsq = dim * dim;

	const player_t opponent = getOpponent(p);

	bool   halfway_trigger = false;
	double score_threshold = -1000.;

	std::vector<std::pair<double, uint32_t> > local_results;
	local_results.resize(dimsq);

	auto lib_it = liberties->begin();

	for(;;) {
		uint64_t now = get_ts_ms();

		if (now >= end_t)
			break;

		if (now >= h_end_t && halfway_trigger == false) {
			halfway_trigger = true;

			double total_scores = 0.;
			int    total_count  = 0;

			for(int i=0; i<dimsq; i++) {
				total_scores += local_results.at(i).first;
				total_count  += local_results.at(i).second;
			}

			score_threshold = total_scores / total_count;

			// printf("set threshold to %f\n", score_threshold);
		}

		int    v             = lib_it->getV();

		// when never played, try get it played
		double current_score = local_results.at(v).second > 0 ? local_results.at(v).first / local_results.at(v).second : 1000000.;

		if (current_score >= score_threshold) {
			Board work(*b);

			play(&work, *lib_it, p);

			auto rc = playout(work, komi, opponent);

			double score = p == P_BLACK ? std::get<0>(rc) - std::get<1>(rc) : std::get<1>(rc) - std::get<0>(rc);

			local_results.at(v).first += score;
			local_results.at(v).second++;
		}

		lib_it++;

		if (lib_it == liberties->end())
			lib_it = liberties->begin();
	}

	std::unique_lock<std::mutex> lck(*all_results_lock);

	for(int i=0; i<dimsq; i++) {
		if (local_results.at(i).second) {
			all_results->at(i).first  += local_results.at(i).first;  // score
			all_results->at(i).second += local_results.at(i).second;  // count
		}
	}
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

	for(int i=0; i<dimsq; i++) {
		if (all_results.at(i).second) {
			double score = all_results.at(i).first / all_results.at(i).second;

			evals->at(i).score += score;

			evals->at(i).valid = true;
		}
	}
}

void purgeKO(const Board & b, const player_t p, std::set<uint64_t> *const seen, std::vector<Vertex> *const liberties)
{
        for(auto it = liberties->begin(); it != liberties->end();) {
                Board temp(b);

                play(&temp, *it, p);

                if (seen->find(temp.getHash()) != seen->end()) {
			*it = liberties->back();
			liberties->pop_back();
		}
                else {
                        it++;
		}
        }
}

std::optional<Vertex> genMove(Board *const b, const player_t & p, const bool doPlay, const double useTime, const double komi, const int nThreads, std::set<uint64_t> *const seen)
{
	dump(*b);

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
				v = temp;
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

double benchmark_1(const Board & in, const unsigned ms, const double komi)
{
	send(true, "# starting benchmark 1: duration: %.3fs, board dimensions: %d, komi: %g", ms / 1000.0, in.getDim(), komi);

	uint64_t start = get_ts_ms();
	uint64_t end   = 0;
	uint64_t n     = 0;
	uint64_t total_puts = 0;

	do {
		auto result = playout(in, komi, P_BLACK);

		total_puts += std::get<2>(result);

		n++;

		end = get_ts_ms();
	}
	while(end - start < ms);

	double td         = (end - start) / 1000.;
	double n_playouts = n / td;
	send(true, "# playouts (total: %lu) per second: %f (%.1f stones on average (total: %lu) or %f stones per second)", n, n_playouts, total_puts / double(n), total_puts, total_puts / td);

	return n_playouts;
}

double benchmark_2(const Board & in, const unsigned ms)
{
	send(true, "# starting benchmark 2: duration: %.3fs, board dimensions: %d", ms / 1000.0, in.getDim());

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

double benchmark_3(const Board & in, const unsigned ms)
{
	send(true, "# starting benchmark 3: duration: %.3fs, board dimensions: %d", ms / 1000.0, in.getDim());

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

bool compareChain(const std::set<Vertex> & a, const std::set<Vertex> & b)
{
	for(auto v : a) {
		if (b.find(v) == b.end())
			return false;
	}

	return true;
}

bool compareChain(const std::vector<Vertex> & a, const std::vector<Vertex> & b)
{
	for(auto v : a) {
		if (std::find(b.begin(), b.end(), v) == b.end())
			return false;
	}

	return true;
}

bool findChain(const std::vector<chain_t *> & chains, const std::vector<Vertex> & search_for)
{
	for(auto c : chains) {
		if (compareChain(c->chain, search_for))
			return true;
	}

	return false;
}

bool findChain(const std::vector<chain_t *> & chains, const std::set<Vertex> & search_for)
{
	for(auto c : chains) {
		if (compareChain(c->liberties, search_for))
			return true;
	}

	return false;
}

bool compareChainT(const std::vector<chain_t *> & chains1, const std::vector<chain_t *> & chains2)
{
	for(auto & c : chains1) {
		if (findChain(chains2, c->chain) == false)
			return false;

		if (findChain(chains2, c->liberties) == false)
			return false;
	}

	return true;
}

void test(const bool verbose)
{
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
			printf("expected score: %f, current: %f\n", b.score, test_score), ok = false;

		ChainMap cm(brd.getDim());
		std::vector<chain_t *> chainsWhite, chainsBlack;
		findChains(brd, &chainsWhite, &chainsBlack, &cm);

		scanEnclosed(brd, &cm, playerToStone(P_WHITE));

		if (b.white_chains.size() != chainsWhite.size())
			printf("white: number of chains mismatch\n"), ok = false;

		if (b.black_chains.size() != chainsBlack.size())
			printf("black: number of chains mismatch\n"), ok = false;

		for(auto ch : b.white_chains) {
			auto white_stones = stringToChain(ch.first, brd.getDim());

			if (findChain(chainsWhite, white_stones) == false)
				printf("white stones mismatch\n"), ok = false;
		}

		for(auto ch : b.white_chains) {
			auto white_liberties = stringToChain(ch.second, brd.getDim());

			if (findChain(chainsWhite, white_liberties) == false)
				printf("white liberties mismatch for %s\n", ch.second.c_str()), ok = false;
		}

		for(auto ch : b.black_chains) {
			auto black_stones = stringToChain(ch.first, brd.getDim());

			if (findChain(chainsBlack, black_stones) == false)
				printf("black stones mismatch\n"), ok = false;
		}

		for(auto ch : b.black_chains) {
			auto black_liberties = stringToChain(ch.second, brd.getDim());

			if (findChain(chainsBlack, black_liberties) == false)
				printf("black liberties mismatch for %s\n", ch.second.c_str()), ok = false;
		}

		if (!ok) {
			dump(brd);

			dump(chainsBlack);

			dump(chainsWhite);

			send(true, "---\n");
		}

		purgeChains(&chainsBlack);
		purgeChains(&chainsWhite);
	}

	// zobrist hashing
	Board b(&z, 9);

	uint64_t startHash = b.getHash();
	if (startHash)
		printf("initial hash (%lx) invalid\n", startHash);

	b.setAt(3, 3, B_BLACK);
	uint64_t firstHash = b.getHash();

	if (firstHash == 0)
		printf("hash did not change\n");

	b.setAt(3, 3, B_WHITE);
	uint64_t secondHash = b.getHash();

	if (secondHash == firstHash)
		printf("hash (%lx) did not change for type\n", secondHash);

	if (secondHash == 0)
		printf("hash became initial\n");

	b.setAt(3, 3, B_EMPTY);
	uint64_t thirdHash = b.getHash();

	if (thirdHash)
		printf("hash (%lx) did not reset\n", thirdHash);

	// "connect()"
	for(auto b : boards) {
		bool ok = true;

		Board brd1 = stringToBoard(b.b);
		Board brd2(brd1);

		dump(brd1);

		assert(brd2.getHash() == brd1.getHash());  // sanity check

		ChainMap cm2(brd2.getDim());
		std::vector<chain_t *> chainsWhite2, chainsBlack2;
		findChains(brd2, &chainsWhite2, &chainsBlack2, &cm2);
		std::vector<Vertex> liberties2;
		findLiberties(cm2, &liberties2, playerToStone(P_BLACK));

		auto move = liberties2.at(0);

		play(&brd1, move, P_BLACK);

		ChainMap cm1(brd1.getDim());
		std::vector<chain_t *> chainsWhite1, chainsBlack1;
		findChains(brd1, &chainsWhite1, &chainsBlack1, &cm1);
		std::vector<Vertex> liberties1;
		findLiberties(cm1, &liberties1, playerToStone(P_BLACK));

		connect(&brd2, &cm2, &chainsWhite2, &chainsBlack2, &liberties2, playerToStone(P_BLACK), move.getX(), move.getY());

		if (brd2.getHash() != brd1.getHash())
			printf("boards mismatch\n"), ok = false;

		if (compareChainT(chainsWhite1, chainsWhite2) == false)
			printf("chainsWhite mismatch\n"), ok = false;

		if (compareChainT(chainsBlack1, chainsBlack2) == false)
			printf("chainsBlack mismatch\n"), ok = false;

		if (compareChain(liberties1, liberties2) == false)
			printf("liberties mismatch\n"), ok = false;

		if (!ok) {
			send(true, " * boards\n");
			dump(brd1);
			dump(brd2);

			send(true, " * chains black\n");
			dump(chainsBlack1);
			dump(chainsBlack2);

			send(true, " * chains white\n");
			dump(chainsWhite1);
			dump(chainsWhite2);

			send(true, "---\n");
		}

		purgeChains(&chainsBlack2);
		purgeChains(&chainsWhite2);

		purgeChains(&chainsBlack1);
		purgeChains(&chainsWhite1);
	}

	printf("--- unittest end ---\n");
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

int main(int argc, char *argv[])
{
	int nThreads = std::thread::hardware_concurrency();

	int dim = 9;

	int c = -1;
	while((c = getopt(argc, argv, "vt:5")) != -1) {
		if (c == 'v')  // console
			setVerbose(true);
		else if (c == 't')
			nThreads = atoi(optarg);
		else if (c == '5')
			dim = 5;
	}

	setbuf(stdout, nullptr);
	setbuf(stderr, nullptr);

	srand(time(nullptr));

	Board   *b    = new Board(&z, dim);

	player_t p    = P_BLACK;
	
	int      pass = 0;

	double timeLeft = -1;

	double komi = 0.;

	int moves_executed = 0;
	int moves_total    = dim * dim;

	std::string sgf = init_sgf(b->getDim());

	std::set<uint64_t> seen;

	for(;;) {
		char buffer[4096] { 0 };
		if (!fgets(buffer, sizeof buffer, stdin))
			break;

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
			timeLeft       = -1;

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
				pops = benchmark_1(*b, atoi(parts.at(1).c_str()), komi);
			else if (parts.at(2) == "2")
				pops = benchmark_2(*b, atoi(parts.at(1).c_str()));
			else if (parts.at(2) == "3")
				pops = benchmark_3(*b, atoi(parts.at(1).c_str()));

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
			timeLeft = atof(parts.at(2).c_str());

			send(false, "=%s", id.c_str());  // TODO
		}
		else if (parts.at(0) == "list_commands") {
			send(false, "=%s name", id.c_str());
			send(false, "=%s version", id.c_str());
			send(false, "=%s boardsize", id.c_str());
			send(false, "=%s clear_board", id.c_str());
			send(false, "=%s play", id.c_str());
			send(false, "=%s genmove", id.c_str());
			send(false, "=%s komi", id.c_str());
			send(false, "=%s quit", id.c_str());
			send(false, "=%s loadsgf", id.c_str());
			send(false, "=%s final_score", id.c_str());
			send(false, "=%s time_settings", id.c_str());
			send(false, "=%s time_left", id.c_str());
		}
		else if (parts.at(0) == "final_score") {
			auto final_score = score(*b, komi);

			send(true, "# black: %f, white: %f", final_score.first, final_score.second);

			send(false, "=%s %s", id.c_str(), scoreStr(final_score).c_str());
		}
		else if (parts.at(0) == "unittest")
			test(parts.size() == 2 ? parts.at(1) == "-v" : false);
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
