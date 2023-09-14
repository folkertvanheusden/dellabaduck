#include <algorithm>
#include <assert.h>
#include <cfloat>
#include <random>
#include <mutex>

#include "board.h"
#include "io.h"
#include "playout.h"
#include "random.h"
#include "str.h"
#include "time.h"
#include "uct.h"


thread_local auto rng = std::default_random_engine {};

uct_node::uct_node(uct_node *const parent, const Board & position, const board_t player, const std::optional<Vertex> & causing_move, const double komi, const std::unordered_set<uint64_t> & seen_in, const int depth) :
	parent(parent),
	position(position),
	player(player),
	causing_move(causing_move),
	komi(komi),
	seen(seen_in),
	depth(depth)
{
	if (causing_move.has_value()) {
		assert(this->position.getAt(causing_move.value()) == board_t::B_EMPTY);

		this->position.startMove();
		this->position.putAt(causing_move.value(), opponentColor(player));
		this->position.finishMove();

		valid = this->seen.insert(this->position.getHash()).second;

		assert(!valid || seen_in.find(this->position.getHash()) == seen_in.end());
	}

	assert(this->seen.size() - seen_in.size() <= 1);

	if (valid) {
		unvisited = this->position.findLiberties(player);

		game_over = unvisited.empty();

		if (!game_over)
			std::shuffle(std::begin(unvisited), std::end(unvisited), rng);
	}
}

uct_node::~uct_node()
{
	for(auto u : children)
		delete u.second;
}

bool uct_node::verify() const
{
	if (!fully_expanded())
		return true;

	if (score > visited)
		return false;

	if (parent && parent->get_player() == get_player())
		return false;

	bool     rc                   = true;
	uint64_t children_visit_count = 0;

	for(auto & u : children) {
		children_visit_count += u.second->get_visit_count();

		if (!u.second->verify())
			rc = false;
	}

/*	if (children_visit_count + 1 != visited) {
		send(true, "# this %p node: %lu, children: %lu\n", this, visited, children_visit_count);

		rc = false;
	} */

	return rc;
}

std::optional<uct_node *> uct_node::add_child(const Vertex & m)
{
	uct_node *new_node = new uct_node(this, position, opponentColor(player), m, komi, seen, depth + 1);

	if (new_node->is_valid()) {
		children.emplace_back(m, new_node);

		return new_node;
	}

	delete new_node;

	return { };
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

	auto first = unvisited.begin();  // back + pop_back

	auto new_node = add_child(*first);

	unvisited.erase(first);

	if (new_node.has_value())
		return new_node.value();

	return nullptr;
}

bool uct_node::fully_expanded() const
{
	return unvisited.empty();
}

uct_node *uct_node::best_uct()
{
	uct_node *best       = nullptr;
	double    best_score = -DBL_MAX;

	for(auto & u : children) {
		if (u.second->is_valid() == false)
			continue;

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
	uct_node *node = this;

	while(node->fully_expanded()) {
		uct_node *next = node->best_uct();

		if (!next)
			break;

		node = next;
	}

	uct_node *chosen = node;

	if (node && node->is_game_over() == false)
		chosen = node->pick_unvisited();

	return chosen;
}

uct_node *uct_node::best_child() const
{
	uct_node *best       = nullptr;
	uint64_t  best_count = 0;

	assert(is_valid());

	for(auto u : children) {
		if (u.second->is_valid() == false)
			continue;

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

double uct_node::playout(const uct_node *const leaf)
{
	board_t p = leaf->get_player();

	auto rc = ::playout(leaf->get_position(), komi, p, seen);

	double result = 0.5;

	if (p == board_t::B_BLACK && std::get<0>(rc) > std::get<1>(rc))
		result = 1.;
	else if (p == board_t::B_WHITE && std::get<0>(rc) < std::get<1>(rc))
		result = 1.;
	else if (p == board_t::B_BLACK && std::get<0>(rc) < std::get<1>(rc))
		result = 0.;
	else if (p == board_t::B_WHITE && std::get<0>(rc) > std::get<1>(rc))
		result = 0.;

	return result;
}

void uct_node::monte_carlo_tree_search()
{
	uct_node *leaf = traverse();

	if (leaf == nullptr)  // ko
		return;

	assert(leaf->is_valid());

	double simulation_result = playout(leaf);

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

std::tuple<std::optional<Vertex>, uint64_t, uint64_t> calculate_move(const Board & b, const board_t p, const uint64_t think_end_time, const double komi, const std::optional<uint64_t> n_limit, const std::unordered_set<uint64_t> & seen)
{
	uct_node *root     = new uct_node(nullptr, b, p, { }, komi, seen, 0);

	uint64_t  n_played = 0;

	for(;;) {
		root->monte_carlo_tree_search();

		n_played++;

		if (get_ts_ms() >= think_end_time || (n_limit.has_value() && n_played >= n_limit.value())) {
			auto best_node      = root->best_child();

			std::optional<Vertex> best_move;
			uint64_t best_count = 0;

			if (best_node) {
				best_move  = best_node->get_causing_move();

				best_count = best_node->get_visit_count();

				assert(best_node->is_valid());

				assert(best_node->verify());
			}

			assert(root->verify());

			delete root;

			// fprintf(stderr, "# n played/s: %.2f\n", n_played * 1000.0 / think_time);

			return { best_move, n_played, best_count };
		}
	}
}
