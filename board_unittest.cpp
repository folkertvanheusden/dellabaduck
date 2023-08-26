#include <cassert>
#include <cstdio>

#include "board.h"

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

		b.undoMoveSet();

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

	printf("All good\n");

	return 0;
}
