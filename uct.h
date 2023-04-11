#include <optional>
#include <vector>

#include "board.h"
#include "vertex.h"


class uct_node
{
private:
	uct_node                 *const parent    { nullptr };
	Board                           position;
	const std::optional<Vertex>     causing_move;

	std::vector<std::pair<Vertex, uct_node *> > children;
	std::vector<Vertex>             unvisited;
	uint64_t                        visited   { 0 };
	double                          score     { 0. };

	uct_node *add_child(const Vertex & m);

	uct_node *get_parent();
	uct_node *pick_unvisited();
	uct_node *traverse();
	uct_node *best_uct();
	void      backpropagate(uct_node *const node, double result);
	bool      fully_expanded();
	double    get_score();
	Board playout(const uct_node *const leaf);

public:
	uct_node(uct_node *const parent, const Board & position, const std::optional<Vertex> & causing_move);
	virtual ~uct_node();

	void         monte_carlo_tree_search();

	const Board  get_position() const;

	uct_node    *best_child() const;

	const Vertex get_causing_move() const;

	const std::vector<std::pair<Vertex, uct_node *> > & get_children() const;
	void         update_stats(const uint64_t visited, const double score);
	uint64_t     get_visit_count();
	double       get_score_count();
};
