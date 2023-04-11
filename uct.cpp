// written by folkert van heusden <mail@vanheusden.com>
// this code is public domain
#include <algorithm>
#include <cfloat>
#include <random>
#include <mutex>

#include "uct.h"


auto produce_seed()
{
	std::vector<unsigned int> random_data(std::mt19937::state_size);

	std::random_device source;
	std::generate(std::begin(random_data), std::end(random_data), [&](){return source();});

	return std::seed_seq(std::begin(random_data), std::end(random_data));
}

thread_local auto mt_seed = produce_seed();
thread_local std::mt19937_64 gen { mt_seed };

uct_node::uct_node(uct_node *const parent, const Board & position, const std::optional<Vertex> & causing_move) :
	parent(parent),
	position(position),
	causing_move(causing_move)
{
	if (causing_move.has_value())
		this->position.makemove(causing_move.value());

	unvisited = this->position.legal_moves();

	if (!unvisited.empty()) {
		std::sort(unvisited.begin(), unvisited.end(), [position](Vertex & a, Vertex & b) {
				auto score_a = position.count_captures(a) + a.is_single();
				auto score_b = position.count_captures(b) + b.is_single();
				return score_a < score_b;  // best move at end of vector
				});
	}
}

uct_node::~uct_node()
{
	for(auto u : children)
		delete u.second;
}

uct_node *uct_node::add_child(const Vertex & m)
{
	uct_node *new_node = new uct_node(this, position, m);

	children.push_back({ m, new_node });

	return new_node;
}

uint64_t uct_node::get_visit_count()
{
	return visited;
}

double uct_node::get_score_count()
{
	return score;
}

void uct_node::update_stats(const uint64_t visited, const double score)
{
	this->visited += visited;
	this->score   += score;
}

double uct_node::get_score()
{
	assert(visited);

	double UCTj = score / visited;

	constexpr double sqrt_2 = sqrt(2.0);

	UCTj += sqrt_2 * sqrt(log(parent->get_visit_count()) / visited);

	return UCTj;
}

uct_node *uct_node::pick_unvisited()
{
	if (unvisited.empty())
		return nullptr;

	uct_node *new_node = add_child(unvisited.back());

	unvisited.pop_back();

	if (unvisited.empty())
		unvisited.shrink_to_fit();

	return new_node;
}

bool uct_node::fully_expanded()
{
	return unvisited.empty();
}

uct_node *uct_node::best_uct()
{
	uct_node *best       = nullptr;
	double    best_score = -DBL_MAX;

	for(auto & u : children) {
		double current_score = u.second->get_score();

		if (current_score > best_score) {
			best_score = current_score;
			best = u.second;
		}
	}

	return best;
}

uct_node *uct_node::traverse()
{
	uct_node *node   = this;

	while(node->fully_expanded()) {
		uct_node *next = node->best_uct();

		if (!next)
			break;

		node = next;
	}

	uct_node *chosen = node;

	if (node && node->get_position().is_gameover() == false)
		chosen = node->pick_unvisited();

	return chosen;
}

uct_node *uct_node::best_child() const
{
	uct_node *best       = nullptr;
	uint64_t  best_count = 0;

	for(auto u : children) {
		uint64_t count = u.second->get_visit_count();

		if (count > best_count) {
			best_count = count;
			best = u.second;
		}
	}

	return best;
}

uct_node *uct_node::get_parent()
{
	return parent;
}

void uct_node::backpropagate(uct_node *const leaf, double result)
{
	uct_node *node = leaf;

	do {
		node->update_stats(1, result);

		result = 1. - result;

		node = node->get_parent();
	}
	while(node);
}

const Board uct_node::get_position() const
{
	return position;
}

Board uct_node::playout(const uct_node *const leaf)
{
	Board position = leaf->get_position();

	while(!position.is_gameover()) {
		auto moves = position.legal_moves();

		std::uniform_int_distribution<> rng(0, moves.size() - 1);

		position.makemove(moves.at(rng(gen)));
	}

	return position;
}

void uct_node::monte_carlo_tree_search()
{
	uct_node *leaf = traverse();

	auto playout_terminal_position = playout(leaf);

	libataxx::Side side = playout_terminal_position.get_turn();

	libataxx::Result result = playout_terminal_position.get_result();

	assert(result != libataxx::Result::None);

	double simulation_result = 0.;

	if ((result == libataxx::Result::BlackWin && side == libataxx::Side::Black) || (result == libataxx::Result::WhiteWin && side == libataxx::Side::White))
		simulation_result = 1.0;
	else if (result == libataxx::Result::Draw)
		simulation_result = 0.5;

	backpropagate(leaf, 1. - simulation_result);
}

const Vertex uct_node::get_causing_move() const
{
	return causing_move.value();
}

const std::vector<std::pair<Vertex, uct_node *> > & uct_node::get_children() const
{
	return children;
}
