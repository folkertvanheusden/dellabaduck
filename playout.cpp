#include <cstdint>
#include <string>
#include <optional>
#include <unordered_set>

#include "board.h"
#include "random.h"
#include "score.h"
#include "str.h"
#include "vertex.h"


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

std::tuple<double, double, int, std::optional<Vertex> > playout(const Board & in, const double komi, const board_t p, const std::unordered_set<uint64_t> & seen_in)
{
	Board b(in);

	const int dim = b.getDim();

	std::unordered_set<uint64_t> seen(seen_in);
	seen.insert(b.getHash());

	int  mc      { 0     };

	bool pass[2] { false };

#ifdef STORE_1_PLAYOUT
	std::string sgf = "(;AP[DellaBaduck]SZ[" + myformat("%d", dim) + "]";

	sgf += myformat(";KM[%f]", komi);
#endif

	board_t for_whom = p;

	bool first_is_pass = false;
	std::optional<Vertex> first;

	std::uniform_int_distribution<> rng_binary(0, 1);

	while(++mc < dim * dim * dim) {
		// did opponent pass?
		if (pass[for_whom != board_t::B_BLACK]) {  // check if it is safe to stop
			auto s = score(b, komi);

			double current_score = for_whom == board_t::B_BLACK ? s.first - s.second : s.second - s.first;

			if (current_score >= 0)  // replace >= if draw is no longer wanted
				return std::tuple<double, double, int, std::optional<Vertex> >(s.first, s.second, mc, first);
		}

	        std::vector<Vertex> liberties = b.findLiberties(for_whom);

		size_t n_liberties = liberties.size();

		if (n_liberties == 0) {
#ifdef STORE_1_PLAYOUT
			sgf += myformat(";%c[pass]", for_whom == board_t::B_BLACK ? 'B' : 'W');
#endif
			break;
		}

		size_t attempt_n = 0;

                std::uniform_int_distribution<> rng(0, n_liberties - 1);
                int o = rng(gen);

		int d = rng_binary(gen) ? 1 : -1;

		int x = 0;
		int y = 0;

		while(attempt_n < n_liberties) {
			assert(b.getAt(liberties.at(o)) == board_t::B_EMPTY);

			x = liberties.at(o).getX();
			y = liberties.at(o).getY();

			if (isInEye(b, x, y, for_whom) == false) {
				b.startMove();
				b.putAt(liberties.at(o), for_whom);
				b.finishMove();

				// and see if it did not produce a ko
				if (seen.insert(b.getHash()).second == true) {
					if (first.has_value() == false && first_is_pass == false) {
						assert(o < n_liberties);
						first = liberties.at(o);

						assert(b.getChain(first.value()).first->getLiberties()->size() > 0);

						assert(for_whom == p);
					}

					// no ko
					break;  // Ok!
				}

				b.undoMoveSet();
			}

			o += d;

			if (o < 0)
				o = n_liberties - 1;
			else if (o >= n_liberties)
				o = 0;

			attempt_n++;
		}

		// all fields tried; pass
		if (attempt_n >= n_liberties) {
			pass[for_whom == board_t::B_BLACK] = true;

#ifdef STORE_1_PLAYOUT
			sgf += myformat(";%c[pass]", for_whom == board_t::B_BLACK ? 'B' : 'W');
#endif

			if (pass[0] && pass[1])
				break;

			for_whom = opponentColor(for_whom);

			if (mc == 1 && first.has_value() == false)
				first_is_pass = true;

			continue;
		}

		pass[0] = pass[1] = false;

#ifdef STORE_1_PLAYOUT
		sgf += myformat(";%c[%c%c]", for_whom == board_t::B_BLACK ? 'B' : 'W', 'a' + x, 'a' + y);
#endif

		for_whom = opponentColor(for_whom);

		assert(first.has_value() || first_is_pass);
	}

	auto s = score(b, komi);

#ifdef STORE_1_PLAYOUT
	sgf += ")";

	printf("%s\n", sgf.c_str());
#endif

	return std::tuple<double, double, int, std::optional<Vertex> >(s.first, s.second, mc, first);
}
