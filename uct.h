#include <optional>
#include <unordered_set>
#include <vector>

#include "board.h"
#include "vertex.h"


class uct_node
{
private:
	uct_node                 *const parent    { nullptr };
	Board                           position;
	const player_t                  player;
	const std::optional<Vertex>     causing_move;
	const double                    komi;

	std::vector<std::pair<Vertex, uct_node *> > children;
	std::vector<Vertex>             unvisited;
	uint64_t                        visited   { 0 };
	double                          score     { 0. };

	bool                            game_over { false };

	uct_node *add_child(const Vertex & m);

	uct_node *get_parent();
	uct_node *pick_unvisited();
	uct_node *traverse();
	uct_node *best_uct();
	void      backpropagate(uct_node *const node, double result);
	bool      fully_expanded() const;
	double    get_score();
	double    playout(const uct_node *const leaf);

public:
	uct_node(uct_node *const parent, const Board & position, const player_t player, const std::optional<Vertex> & causing_move, const double komi);
	virtual ~uct_node();

	bool         is_game_over() const { return game_over; }

	player_t     get_player() const { return player; }

	void         monte_carlo_tree_search();

	const Board  get_position() const;

	uct_node    *best_child() const;

	bool         verify() const;

	const Vertex get_causing_move() const;

	const std::vector<std::pair<Vertex, uct_node *> > & get_children() const;
	void         update_stats(const uint64_t visited, const double score);
	uint64_t     get_visit_count();
	double       get_score_count();
};

Vertex calculate_move(const Board & b, const player_t p, const unsigned think_time, const double komi);
