#include <cassert>
#include <cstdio>

#include "board.h"
#include "str.h"

void dump(const Board & b)
{
        const int dim = b.getDim();

        std::string line;

        for(int y=dim - 1; y>=0; y--) {
                line = myformat("# %2d | ", y + 1);

		// which stone
                for(int x=0; x<dim; x++) {
                        board_t bv = b.getAt(x, y);

                        if (bv == B_EMPTY)
                                line += ".";
                        else if (bv == B_BLACK)
                                line += "x";
                        else if (bv == B_WHITE)
                                line += "o";
                        else
                                line += "!";
                }

		line += "   ";

		// chain number
                for(int x=0; x<dim; x++) {
			auto chain = b.getChainConst(Vertex(x, y, dim));

			line += myformat("%4d", chain.second);
                }

		line += "   ";

		// liberty count
                for(int x=0; x<dim; x++) {
			auto chain = b.getChainConst(Vertex(x, y, dim));

			if (chain.first)
				line += myformat(" %zu", chain.first->getLiberties()->size());
			else
				line += "  ";
                }

                printf("%s\n", line.c_str());
        }

        line = "#      ";

        for(int x=0; x<dim; x++) {
                int xc = 'A' + x;

                if (xc >= 'I')
                        xc++;

                line += myformat("%c", xc);
        }

        printf("%s\n", line.c_str());
}

int main(int argc, char *argv[])
{
	Zobrist z(9);

	// test == function for empty boards
	{
		Board a(&z, 9), b(&z, 9);
		assert(a == b);
	}

	// test != function for empty boards of different size
	{
		Board a(&z, 9), b(&z, 19);
		assert(a != b);
	}

	// test != function for board with a stone on it (without finish)
	{
		Board a(&z, 9), b(&z, 9);
		b.startMove();
		b.putAt(Vertex(0, 0, 9), B_BLACK);
		assert(a != b);
	}

	// test != function for board with a stone on it (with finish)
	{
		Board a(&z, 9), b(&z, 9);
		b.startMove();
		b.putAt(Vertex(0, 0, 9), B_BLACK);
		b.finishMove();
		assert(a != b);
	}

	// test == function for board with a stone on it and then undo
	{
		Board a(&z, 9), b(&z, 9);
		b.startMove();
		b.putAt(Vertex(0, 0, 9), B_BLACK);
		b.finishMove();

		assert(b.getUndoDepth() == 1);

		b.undoMoveSet();

		assert(a == b);
	}

	// test == function for board with a stone on it and then undo to empty
	{
		Board a(&z, 9), b(&z, 9);
		b.startMove();
		b.putAt(Vertex(0, 0, 9), B_BLACK);
		b.putAt(Vertex(3, 2, 9), B_WHITE);
		b.putAt(Vertex(1, 1, 9), B_BLACK);
		b.putAt(Vertex(7, 8, 9), B_WHITE);
		b.finishMove();

		b.undoMoveSet();

		assert(a == b);
	}

	// test == function for board with a stone on it and then undo to a non-empty
	{
		Board a(&z, 9), b(&z, 9);

		a.startMove();
		a.putAt(Vertex(1, 3, 9), B_BLACK);
		a.putAt(Vertex(4, 4, 9), B_WHITE);
		a.finishMove();

		b.startMove();
		b.putAt(Vertex(1, 3, 9), B_BLACK);
		b.putAt(Vertex(4, 4, 9), B_WHITE);
		b.finishMove();

		b.startMove();
		b.putAt(Vertex(0, 0, 9), B_BLACK);
		b.putAt(Vertex(3, 2, 9), B_WHITE);
		b.putAt(Vertex(1, 1, 9), B_BLACK);
		b.putAt(Vertex(7, 8, 9), B_WHITE);
		b.finishMove();

		assert(a.getUndoDepth() == 1);
		assert(b.getUndoDepth() == 2);

		b.undoMoveSet();

		assert(b.getUndoDepth() == 1);

		assert(a == b);
	}

	// test board assignment
	{
		Board a(&z, 19), b(&z, 9);

		b.startMove();
		b.putAt(Vertex(0, 0, 9), B_BLACK);
		b.putAt(Vertex(3, 2, 9), B_WHITE);
		b.putAt(Vertex(1, 1, 9), B_BLACK);
		b.putAt(Vertex(7, 8, 9), B_WHITE);
		b.finishMove();

		a = b;

		assert(a == b);
	}

	// see if a chain is created when a stone is placed
	{
		Board a(&z, 9);

		a.startMove();
		a.putAt(Vertex(1, 1, 9), B_BLACK);
		a.finishMove();

		assert(a.getUndoDepth() == 1);

		auto chain = a.getChain(Vertex(1, 1, 9));

		assert(chain.first  != nullptr);
		assert(chain.second != 0);

		assert(a.getBlackChains()->size() == 1);
		assert(a.getWhiteChains()->empty());

		assert(a.getBlackChains()->begin()->second->getLiberties()->size() == 4);
	}

	// see if a chain is removed when undo is invoked
	{
		Board a(&z, 9);

		a.startMove();
		a.putAt(Vertex(1, 1, 9), B_BLACK);
		a.finishMove();

		assert(a.getUndoDepth() == 1);

		a.undoMoveSet();

		assert(a.getUndoDepth() == 0);

		auto chain = a.getChain(Vertex(1, 1, 9));

		assert(chain.first  == nullptr);
		assert(chain.second == 0);

		assert(a.getAt(Vertex(1, 1, 9)) == B_EMPTY);

		assert(a.getBlackChains()->empty());
		assert(a.getWhiteChains()->empty());
	}

	// see if a chain is removed that others are not affected
	{
		Board a(&z, 9);

		a.startMove();
		a.putAt(Vertex(1, 1, 9), B_BLACK);
		a.finishMove();

		a.startMove();
		a.putAt(Vertex(7, 7, 9), B_BLACK);
		a.finishMove();

		a.undoMoveSet();

		auto chain = a.getChain(Vertex(1, 1, 9));

		assert(chain.first  != nullptr);
		assert(chain.second != 0);

		chain = a.getChain(Vertex(7, 7, 9));

		assert(chain.first  == nullptr);
		assert(chain.second == 0);

		assert(a.getAt(Vertex(7, 7, 9)) == B_EMPTY);

		assert(a.getBlackChains()->size() == 1);
		assert(a.getWhiteChains()->empty());

		assert(a.getBlackChains()->begin()->second->getLiberties()->size() == 4);
	}

	// see if chains are merged
	{
		Board a(&z, 9);

		a.startMove();
		a.putAt(Vertex(1, 1, 9), B_BLACK);
		a.finishMove();

		a.startMove();
		a.putAt(Vertex(3, 1, 9), B_BLACK);
		a.finishMove();

		a.startMove();
		a.putAt(Vertex(2, 1, 9), B_BLACK);
		a.finishMove();

		assert(a.getUndoDepth() == 3);

		auto chain1 = a.getChain(Vertex(1, 1, 9));
		auto chain2 = a.getChain(Vertex(2, 1, 9));
		auto chain3 = a.getChain(Vertex(3, 1, 9));

		assert(chain1.first  == chain2.first  && chain1.first  == chain3.first);
		assert(chain1.second == chain2.second && chain1.second == chain3.second);

		assert(chain1.first  != nullptr);
		assert(chain1.second != 0);

		assert(a.getBlackChains()->size() == 1);
		assert(a.getWhiteChains()->empty());

		assert(a.getBlackChains()->begin()->second->getLiberties()->size() == 8);
	}

	// see if a chain is removed when a chain is dead
	{
		Board a(&z, 9);

		a.startMove();
		a.putAt(Vertex(4, 4, 9), B_BLACK);
		a.finishMove();

		a.startMove();
		a.putAt(Vertex(4, 3, 9), B_WHITE);  // above
		a.putAt(Vertex(4, 5, 9), B_WHITE);  // below
		a.putAt(Vertex(3, 4, 9), B_WHITE);  // left
		a.putAt(Vertex(5, 4, 9), B_WHITE);  // right
		a.finishMove();

		assert(a.getUndoDepth() == 2);

		auto chain = a.getChain(Vertex(4, 4, 9));

		assert(chain.first  == nullptr);
		assert(chain.second == 0);

		assert(a.getBlackChains()->empty());
		assert(a.getWhiteChains()->size() == 4);

		for(auto & c: *a.getBlackChains())
			assert(c.second->getLiberties()->size() == 4);
	}

	// verify restored chain and liberty counts after undo
	{
		Board a(&z, 9);

		Vertex testV(4, 4, 9);
		a.startMove();
		a.putAt(testV, B_BLACK);
		a.finishMove();

		auto prev_data = a.getChain(testV);
		assert(prev_data.first  != nullptr);
		assert(prev_data.second != 0);

		a.startMove();
		a.putAt(Vertex(4, 3, 9), B_WHITE);  // above
		a.putAt(Vertex(4, 5, 9), B_WHITE);  // below
		a.putAt(Vertex(3, 4, 9), B_WHITE);  // left
		a.finishMove();

		a.startMove();
		a.putAt(Vertex(5, 4, 9), B_WHITE);  // right
		a.finishMove();

		assert(a.getUndoDepth() == 3);

		a.undoMoveSet();

		assert(a.getUndoDepth() == 2);

		dump(a);

		assert(a.getAt(testV) == B_BLACK);  // board check
		assert(a.getChain(testV).first  == prev_data.first);  // chain map check (pointer)
		assert(a.getChain(testV).second == prev_data.second);  // chain map check (index)
		assert(a.getChain(testV).first->getLiberties()->size() == 1);

		assert(a.getChain(Vertex(4, 3, 9)).first->getLiberties()->size() == 3);
		assert(a.getChain(Vertex(4, 5, 9)).first->getLiberties()->size() == 3);
		assert(a.getChain(Vertex(3, 4, 9)).first->getLiberties()->size() == 3);
	}

	printf("All good\n");

	return 0;
}
