#include <string>

#include "board.h"
#include "str.h"


std::string init_sgf(const int dim)
{
        return "(;AP[DellaBaduck]SZ[" + myformat("%d", dim) + "]";
}

std::string dump_to_sgf(const Board & b, const double komi, const bool with_end)
{
        int         dim = b.getDim();
        std::string sgf = init_sgf(dim);
                        
        sgf += myformat(";KM[%f]", komi);
                
        for(int y=0; y<dim; y++) {
                for(int x=0; x<dim; x++) {
                        auto v     = Vertex(x, y, dim);

                        auto stone = b.getAt(v.getV());

                        if (stone == board_t::B_EMPTY)
                                continue;

                        sgf += myformat(";%c[%s]", stone == board_t::B_BLACK ? 'B' : 'W', v.to_str(true).c_str());
                }
        }

        return sgf + (with_end ? ")" : "");
}

