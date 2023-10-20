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
	const board_t                   player;
	const std::optional<Vertex>     causing_move;
	const double                    komi;
	std::unordered_set<uint64_t>    seen;
	bool                            valid     { true };

	std::vector<uct_node>           children;
	std::vector<Vertex>             unvisited;
	uint64_t                        visited   { 0  };
	double                          score     { 0. };

	bool                            game_over { false };
	bool                            first     { true  };

	std::optional<uct_node *> add_child(const Vertex & m);

	uct_node *get_parent();
	uct_node *pick_unvisited();
	uct_node *traverse();
	uct_node *best_uct();
	void      backpropagate(uct_node *const node, double result);
	bool      fully_expanded() const;
	double    get_score();
	double    playout(const uct_node *const leaf);

public:
	uct_node(uct_node *const parent, Board & position, const board_t player, const std::optional<Vertex> & causing_move, const double komi, std::unordered_set<uint64_t> & seen_in);
	virtual ~uct_node();

	bool         is_valid() const { return valid; }

	bool         is_game_over() const { return game_over; }

	board_t      get_player() const { return player; }

	void         monte_carlo_tree_search();

	const Board &get_position() const;

	const uct_node *best_child() const;
	auto         get_children() const;

	bool         verify() const;

	const Vertex get_causing_move() const;

	void         update_stats(const uint64_t visited, const double score);
	uint64_t     get_visit_count() const;
	double       get_score_count() const;
};

std::tuple<std::optional<Vertex>, uint64_t, uint64_t, std::vector<std::tuple<Vertex, uint64_t, double> > > calculate_move(const Board & b, const board_t p, const uint64_t think_end_time, const uint64_t think_end_time_extra, const double komi, const std::optional<uint64_t> n_limit, const std::unordered_set<uint64_t> & seen);
