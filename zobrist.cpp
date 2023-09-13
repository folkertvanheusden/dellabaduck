#include "random.h"
#include "zobrist.h"


Zobrist::Zobrist(const int dim)
{
	setDim(dim);
}

Zobrist::~Zobrist()
{
}

void Zobrist::setDim(const int dim)
{
	size_t newN = dim * dim * 2;

	for(size_t i=rngs.size(); i<newN; i++)
		rngs.push_back(distribution(gen));
}

uint64_t Zobrist::get(const int nr, const bool black) const
{
#ifdef SITUATIONAL_SUPERKO
	return rngs.at(nr * 2 + black);
#else
	return rngs.at(nr);  // TT & CGOS
#endif
}
